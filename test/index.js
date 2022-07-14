'use strict';
/*global it describe*/

const assert = require('assert');
const Bluebird = require('bluebird');
const filedisk = require('file-disk');
const { createReadStream } = require('fs');
const pathModule = require('path');
const stream = require('stream');

const ext2fs = require('..');

// Each image contains 5 files named 1, 2, 3, 4, 5 and containing
// 'one\n', 'two\n', 'three\n', 'four\n', 'five\n' respectively.

const IMAGES = {
	'ext2': 'ext2.img',
	'ext3': 'ext3.img',
	'ext4': 'ext4.img',
	'ext4-4k-block-size': 'ext4-4k-block-size.img'
};

function humanFileMode(fs, stats) {
	const result = [];
	result.push(stats.isDirectory() ? 'd' : '-');
	let constant, enabled;
	for (let actor of ['USR', 'GRP', 'OTH']) {
		for (let action of ['R', 'W', 'X']) {
			constant = fs.constants[`S_I${action}${actor}`];
			enabled = ((stats.mode & constant) !== 0);
			result.push(enabled ? action.toLowerCase() : '-');
		}
	}
	return result.join('');
}

function testOnAllDisks(fn) {
	for (const name of Object.keys(IMAGES)) {
		it(name, async () => {
			const path = pathModule.join(__dirname, 'fixtures', IMAGES[name]);
			await filedisk.withOpenFile(path, 'r', async (fd) => {
				const disk = new filedisk.FileDisk(fd, true, true);
				// `disk.imageName` will be useful in tests that have different
				// results depending on the image.
				disk.imageName = name;
				await fn(disk);
			});
		});
	}
}

function testOnAllDisksMount(fn) {
	testOnAllDisks(async (disk) => {
		// eslint-disable-line no-unused-vars
		await ext2fs.withMountedDisk(disk, 0, async (fs, fsPromises) => {
			// Might be useful to get the disk name
			fs.disk = disk;
			await fn(Bluebird.promisifyAll(fs, { multiArgs: true }), fsPromises);
		});
	});
}

function readStream(stream, buffer = false) {
	return new Promise((resolve, reject) => {
		const chunks = [];
		stream.on('error', reject);
		stream.on('close', () => {
			if (buffer) {
				resolve(Buffer.concat(chunks));
			} else {
				resolve(chunks.join(''));
			}
		});
		stream.on('data', (chunk) => {
			chunks.push(chunk);
		});
	});
}

function waitStream(stream) {
	return new Promise((resolve, reject) => {
		stream.on('error', reject);
		stream.on('close', resolve);
	});
}

function createReadableStreamFromString(s) {
	const readable = new stream.Readable();
	readable._read = () => {};
	readable.push(s);
	readable.push(null);
	return readable;
}

describe('ext2fs', () => {
	describe('disk errors', () => {
		testOnAllDisks(async (disk) => {
			disk.read = () => {
				throw new Error("can't read");
			};
			try {
				await ext2fs.mount(disk, 0);
				assert(false);
			} catch(err) {
				assert.strictEqual(err.errno, 29);
				assert.strictEqual(err.code, 'EIO');
			}
		});
	});

	describe('offset', () => {
		it('offset', async () => {
			const path = pathModule.join(__dirname, 'fixtures', IMAGES['ext4']);
			const data = await readStream(createReadStream(path), true);
			const offset = 2048;
			const buffer = Buffer.allocUnsafe(data.length + offset);
			data.copy(buffer, offset);
			const disk = new filedisk.BufferDisk(buffer, true, true);
			await ext2fs.withMountedDisk(disk, offset, async (fs) => {
				fs = Bluebird.promisifyAll(fs, { multiArgs: true });
				const [files] = await fs.readdirAsync('/');
				files.sort();
				assert.deepEqual(files, [ '1', '2', '3', '4', '5', 'lost+found' ]);
			});
		});
	});

	describe('mount, open, read, close, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const buffer = Buffer.allocUnsafe(4);
			const [fd] = await fs.openAsync('/1', 'r');
			const [bytesRead, buf] = await fs.readAsync(fd, buffer, 0, 4, 0);
			assert.strictEqual(bytesRead, 4);
			assert.strictEqual(buf.toString(), 'one\n');
			await fs.closeAsync(fd);
		});
	});

	describe('mount, stat, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const [stats] = await fs.statAsync('/2');
			assert.strictEqual(stats.dev, 0);
			assert.strictEqual(stats.mode, 33188);
			assert.strictEqual(stats.nlink, 1);
			assert.strictEqual(stats.uid, 1000);
			assert.strictEqual(stats.gid, 1000);
			assert.strictEqual(stats.rdev, 0);
			if (fs.disk.imageName === 'ext4-4k-block-size') {
				assert.strictEqual(stats.blksize, 4096);
				assert.strictEqual(stats.blocks, 8);
			} else {
				assert.strictEqual(stats.blksize, 1024);
				assert.strictEqual(stats.blocks, 2);
			}
			assert.strictEqual(stats.size, 4);
			assert.strictEqual(
				stats.atime.getTime(),
				(new Date('2017-05-23T18:56:45.000Z')).getTime()
			);
			assert.strictEqual(
				stats.mtime.getTime(),
				(new Date('2017-05-22T13:02:28.000Z')).getTime()
			);
			assert.strictEqual(
				stats.ctime.getTime(),
				(new Date('2017-05-23T18:56:47.000Z')).getTime()
			);
			assert.strictEqual(
				stats.birthtime.getTime(),
				(new Date('2017-05-23T18:56:47.000Z')).getTime()
			);
		});
	});

	describe('mount, open, fstat, close, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/2', fs.constants.O_RDONLY | fs.constants.O_NOATIME);
			const [stats] = await fs.fstatAsync(fd);
			assert.strictEqual(stats.dev, 0);
			assert.strictEqual(stats.mode, 33188);
			assert.strictEqual(stats.nlink, 1);
			assert.strictEqual(stats.uid, 1000);
			assert.strictEqual(stats.gid, 1000);
			assert.strictEqual(stats.rdev, 0);
			if (fs.disk.imageName === 'ext4-4k-block-size') {
				assert.strictEqual(stats.blksize, 4096);
				assert.strictEqual(stats.blocks, 8);
			} else {
				assert.strictEqual(stats.blksize, 1024);
				assert.strictEqual(stats.blocks, 2);
			}
			assert.strictEqual(stats.size, 4);
			assert.strictEqual(
				stats.atime.getTime(),
				(new Date('2017-05-23T18:56:45.000Z')).getTime()
			);
			assert.strictEqual(
				stats.mtime.getTime(),
				(new Date('2017-05-22T13:02:28.000Z')).getTime()
			);
			assert.strictEqual(
				stats.ctime.getTime(),
				(new Date('2017-05-23T18:56:47.000Z')).getTime()
			);
			assert.strictEqual(
				stats.birthtime.getTime(),
				(new Date('2017-05-23T18:56:47.000Z')).getTime()
			);
			await fs.closeAsync(fd);
		});
	});

	describe('mount, open, write, fstat, close, open, read, fstat, close, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const string = 'hello';
			const buf = Buffer.from(string);
			const [fd] = await fs.openAsync('/2', 'w');
			await fs.writeAsync(fd, buf, 0, buf.length, 0);
			const [stats] = await fs.fstatAsync(fd);
			// ctime, mtime and birthtime should change
			const now = Date.now();
			assert(now - stats.ctime.getTime() < 3000);
			assert(now - stats.mtime.getTime() < 3000);
			assert(now - stats.birthtime.getTime() < 3000);
			assert.strictEqual(stats.size, 5);
			await fs.closeAsync(fd);
			const [fd2] = await fs.openAsync('/2', 'r');
			const [bytesRead, buf2] = await fs.readAsync(fd2, buf, 0, buf.length, 0);
			assert.strictEqual(bytesRead, 5);
			assert.strictEqual(buf2.toString(), string);
			const [stats2] = await fs.fstatAsync(fd2);
			assert(Date.now() - stats2.atime.getTime() < 1000);
			assert.strictEqual(stats2.size, 5);
			await fs.closeAsync(fd2);
		});
	});

	describe('mount, open, write string, read, close, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const string = 'hello';
			const buffer = Buffer.alloc(string.length);
			const [fd] = await fs.openAsync('/9', 'w+');
			const [bytesWritten, s] = await fs.writeAsync(fd, string);
			assert.strictEqual(bytesWritten, string.length);
			assert.strictEqual(s, string);
			const [bytesRead, buf] = await fs.readAsync(fd, buffer, 0, buffer.length, 0);
			assert.strictEqual(bytesRead, 5);
			assert.strictEqual(buf.toString(), string);
			await fs.closeAsync(fd);
		});
	});

	describe('mount, create, write, fstat, close, open, fstat, read, close, umount', () => {
		const path = '/6';
		const content = 'six\n';
		testOnAllDisksMount(async (fs) => {
			const buf = Buffer.from(content);
			const [fd] = await fs.openAsync(path, 'w+', 0o777);
			const [bytesWritten] = await fs.writeAsync(fd, buf, 0, buf.length, 0);
			assert.strictEqual(bytesWritten, buf.length);
			assert.strictEqual(buf.toString(), content);
			const [statsBeforeClose] = await fs.fstatAsync(fd);
			await fs.closeAsync(fd);
			const [fd2] = await fs.openAsync(path, 'r');
			const [stats] = await fs.fstatAsync(fd2);
			// compare the 2 Stats objects
			let value, otherValue;
			for (let key of Object.keys(statsBeforeClose)) {
				value = statsBeforeClose[key];
				otherValue = stats[key];
				if (value instanceof Date) {
					value = value.getTime();
					otherValue = otherValue.getTime();
				}
				assert.strictEqual(value, otherValue);
			}
			assert(stats.isFile());
			assert.strictEqual(stats.dev, 0);
			assert.strictEqual(stats.nlink, 1);
			assert.strictEqual(stats.uid, 0);
			assert.strictEqual(stats.gid, 0);
			assert.strictEqual(stats.rdev, 0);
			if (fs.disk.imageName === 'ext4-4k-block-size') {
				assert.strictEqual(stats.blocks, 8);
			} else {
				assert.strictEqual(stats.blocks, 2);
			}
			assert.strictEqual(stats.size, content.length);
			assert.strictEqual(humanFileMode(fs, stats), '-rwxrwxrwx');
			const now = Date.now();
			assert(now - stats.atime.getTime() < 3000);
			assert(now - stats.ctime.getTime() < 3000);
			assert(now - stats.mtime.getTime() < 3000);
			assert(now - stats.birthtime.getTime() < 3000);
			buf.fill(0);
			const [bytesRead] = await fs.readAsync(fd2, buf, 0, 1024, 0);
			assert.strictEqual(bytesRead, content.length);
			assert.strictEqual(buf.toString(), content);
			await fs.closeAsync(fd2);
		});
	});

	describe('mount, readFile, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const [data] = await fs.readFileAsync('/1', 'utf8');
			assert.strictEqual(data, 'one\n');
		});
	});

	describe('mount, readdir, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const [filenames] = await fs.readdirAsync('/');
			filenames.sort();
			assert.deepEqual(
				filenames,
				[ '1', '2', '3', '4', '5', 'lost+found' ]
			);
		});
	});

	describe('mount, create, write 1M, read 1M, close, umount', () => {
		testOnAllDisksMount(async (fs) => {
			const size = Math.pow(1024, 2);
			const buf = Buffer.allocUnsafe(size);
			buf.fill(1);
			const [fd] = await fs.openAsync('/8', 'w+');
			const [bytesWritten] = await fs.writeAsync(fd, buf, 0, size, 0);
			assert.strictEqual(bytesWritten, size);
			buf.fill(0);
			const [bytesRead] = await fs.readAsync(fd, buf, 0, size, 0);
			const buf2 = Buffer.allocUnsafe(size);
			buf2.fill(1);
			assert.strictEqual(bytesRead, size);
			assert(buf.equals(buf2));
			await fs.closeAsync(fd);
		});
	});


	describe('open non existent file for reading', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.openAsync('/7', 'r');
				assert(false);
			} catch (err) {
				assert.strictEqual(err.errno, 44);
				assert.strictEqual(err.code, 'ENOENT');
			}
		});
	});

	describe('create file in non existent folder', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.openAsync('/7/8', 'w');
				assert(false);
			} catch(err) {
				assert.strictEqual(err.errno, 54);
				assert.strictEqual(err.code, 'ENOTDIR');
			}
		});
	});

	describe('rmdir', () => {
		testOnAllDisksMount(async (fs) => {
			await fs.rmdirAsync('/lost+found');
			const [files] = await fs.readdirAsync('/');
			files.sort();
			assert.deepEqual(files, [ '1', '2', '3', '4', '5' ]);
		});
	});

	describe('rmdir a folder that does not exist', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.rmdirAsync('/no-such-folder');
				assert(false);
			} catch(error) {
				assert.strictEqual(error.code, 'ENOENT');
				assert.strictEqual(error.errno, 44);
			}
		});
	});

	describe('rmdir a file', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.rmdirAsync('/1');
				assert(false);
			} catch(error) {
				assert.strictEqual(error.code, 'ENOTDIR');
				assert.strictEqual(error.errno, 54);
			}
		});
	});

	describe('unlink', () => {
		testOnAllDisksMount(async (fs) => {
			await fs.unlinkAsync('/1');
			await fs.unlinkAsync(Buffer.from('/2'));
			const [files] = await fs.readdirAsync('/');
			files.sort();
			assert.deepEqual(files, [ '3', '4', '5', 'lost+found' ]);
		});
	});

	describe('unlink a file that does not exist', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.unlinkAsync('/no-such-file');
				assert(false);
			} catch(error) {
				assert.strictEqual(error.code, 'ENOENT');
				assert.strictEqual(error.errno, 44);
			}
		});
	});

	describe('unlink a directory', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				const dirname = '/mydir';
				await fs.mkdirAsync(dirname);
				await fs.unlinkAsync(dirname);
				assert(false);
			} catch(error) {
				assert.strictEqual(error.code, 'EISDIR');
				assert.strictEqual(error.errno, 31);
			}
		});
	});

	describe('access', () => {
		testOnAllDisksMount(async (fs) => {
			await fs.accessAsync('/1');
		});
	});

	describe('execute access on a file that can not be executed', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.accessAsync('/1', fs.constants.X_OK);
				assert(false);
			} catch(error) {
				assert.strictEqual(error.code, 'EACCES');
				assert.strictEqual(error.errno, 2);
			}
		});
	});

	describe('mkdir', () => {
		testOnAllDisksMount(async (fs) => {
			await fs.mkdirAsync('/new-folder');
			const [files] = await fs.readdirAsync('/');
			files.sort();
			assert.deepEqual(
				files,
				[ '1', '2', '3', '4', '5', 'lost+found', 'new-folder' ]
			);
		});
	});

	describe('mkdir with slashes at the end of the path', () => {
		testOnAllDisksMount(async (fs) => {
			await fs.mkdirAsync(Buffer.from('/new-folder//////'));
			const [files] = await fs.readdirAsync('/');
			files.sort();
			assert.deepEqual(
				files,
				[ '1', '2', '3', '4', '5', 'lost+found', 'new-folder' ]
			);
		});
	});

	describe('unlink in a directory that is not /', () => {
		testOnAllDisksMount(async (fs) => {
			await fs.mkdirAsync('/new-folder-2');
			const [files] = await fs.readdirAsync('/');
			files.sort();
			assert.deepEqual(
				files,
				[ '1', '2', '3', '4', '5', 'lost+found', 'new-folder-2' ]
			);
			// Also test trailing slashes removal
			await fs.writeFileAsync('/new-folder-2/filename////', 'some-data');
			const [files2] = await fs.readdirAsync('/new-folder-2');
			assert.deepEqual(files2, ['filename']);
			await fs.unlinkAsync('/new-folder-2/filename');
			const [files3] = await fs.readdirAsync('/new-folder-2');
			assert.deepEqual(files3, []);
		});
	});

	describe('mkdir specific mode', () => {
		testOnAllDisksMount(async (fs) => {
			await fs.mkdirAsync('/new-folder', 0o467);
			const [files] = await fs.readdirAsync('/');
			files.sort();
			assert.deepEqual(
				files,
				[ '1', '2', '3', '4', '5', 'lost+found', 'new-folder' ]
			);
			const [stats] = await fs.statAsync('/new-folder');
			assert.strictEqual(humanFileMode(fs, stats), 'dr--rw-rwx');
		});
	});

	describe('symlink read/write', () => {
		const target = '/config.txt';
		const content = 'content\n';
		const content2 = 'content2\n';
		const encoding = 'utf8';
		const symlink = '/symconfig.txt';
		testOnAllDisksMount(async (fs) => {
			// symlink content should be the same as the
			// original file content after reading and writing
			await fs.writeFileAsync(target, content, encoding);
			await fs.symlinkAsync(target, symlink);
			const [data] = await fs.readFileAsync(symlink, encoding);
			assert.strictEqual(data, content);
			const [fd] = await fs.openAsync(symlink, 'w+');
			await fs.writeAsync(fd, content2);
			await fs.closeAsync(fd);
			const [data2] = await fs.readFileAsync(symlink, encoding);
			assert.strictEqual(data2, content2);
			const [stat] = await fs.lstatAsync(symlink);
			assert.strictEqual(stat.isSymbolicLink(), true);
		});
	});

	describe('write in a readonly file', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/1', 'r');
			try {
				await fs.writeAsync(fd, 'two');
				assert(false);
			} catch(error) {
				assert.strictEqual(error.errno, 8);
				assert.strictEqual(error.code, 'EBADF');
			}
			await fs.closeAsync(fd);
		});
	});

	describe('read a writeonly file', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/1', 'w');
			try {
				await fs.readFileAsync(fd, 'utf8');
				assert(false);
			} catch(error) {
				assert.strictEqual(error.errno, 8);
				assert.strictEqual(error.code, 'EBADF');
			}
			await fs.closeAsync(fd);
		});
	});

	describe('append mode', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/1', 'a+');
			const text = 'two\n';
			const [bytesWritten, data] = await fs.writeAsync(fd, text);
			assert.strictEqual(bytesWritten, text.length);
			assert.strictEqual(data, text);
			const buffer = Buffer.alloc(16);
			const [bytesRead, data2] = await fs.readAsync(fd, buffer, 0, buffer.length, 0);
			assert.strictEqual(bytesRead, 8);
			const dataStr = data2.slice(0, bytesRead).toString();
			assert.strictEqual(dataStr, 'one\ntwo\n');
			const [content] = await fs.readFileAsync('/1', 'utf8');
			assert.strictEqual(content, 'one\ntwo\n');
			await fs.closeAsync(fd);
		});
	});

	describe('readdir a folder that does not exist', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.readdirAsync('/no-such-folder');
				assert(false);
			} catch(error) {
				assert.strictEqual(error.errno, 44);
				assert.strictEqual(error.code, 'ENOENT');
			}
		});
	});

	describe('fchmod', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/1', 'r');
			await fs.fchmodAsync(fd, 0o777);
			const [stats] = await fs.fstatAsync(fd);
			assert.strictEqual(humanFileMode(fs, stats), '-rwxrwxrwx');
			await fs.closeAsync(fd);
		});
	});

	describe('fchmod 2', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/1', 'r');
			await fs.fchmodAsync(fd, 0o137);
			const [stats] = await fs.fstatAsync(fd);
			assert.strictEqual(humanFileMode(fs, stats), '---x-wxrwx');
			await fs.closeAsync(fd);
		});
	});

	describe('lchmod', () => {
		const target = '/usr/bin/chmod';
		const linkpath = '/1.link';
		testOnAllDisksMount(async (fs) => {
			await fs.symlinkAsync(target, linkpath);
			await fs.lchmodAsync(linkpath, 0o137);
			const [lstats] = await fs.lstatAsync(linkpath);
			assert.strictEqual(humanFileMode(fs, lstats), '---x-wxrwx');
		});
	});

	describe('fchmod a folder', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/lost+found', 'r');
			await fs.fchmodAsync(fd, 0o137);
			const [stats] = await fs.fstatAsync(fd);
			assert.strictEqual(humanFileMode(fs, stats), 'd--x-wxrwx');
			await fs.closeAsync(fd);
		});
	});

	describe('chmod', () => {
		testOnAllDisksMount(async (fs) => {
			const path = '/1';
			await fs.chmodAsync(path, 0o777);
			const [stats] = await fs.statAsync(path);
			assert.strictEqual(humanFileMode(fs, stats), '-rwxrwxrwx');
		});
	});

	describe('chmod 2', () => {
		testOnAllDisksMount(async (fs) => {
			const path = '/1';
			await fs.chmodAsync(path, 0o137);
			const [stats] = await fs.statAsync(path);
			assert.strictEqual(humanFileMode(fs, stats), '---x-wxrwx');
		});
	});

	describe('chmod a folder', () => {
		testOnAllDisksMount(async (fs) => {
			const path = '/lost+found';
			await fs.chmodAsync(path, 0o137);
			const [stats] = await fs.statAsync(path);
			assert.strictEqual(humanFileMode(fs, stats), 'd--x-wxrwx');
		});
	});

	describe('fchown', () => {
		testOnAllDisksMount(async (fs) => {
			const [fd] = await fs.openAsync('/1', 'r');
			await fs.fchownAsync(fd, 2000, 3000);
			const [stats] = await fs.fstatAsync(fd);
			assert.strictEqual(stats.uid, 2000);
			assert.strictEqual(stats.gid, 3000);
			await fs.closeAsync(fd);
		});
	});

	describe('chown', () => {
		testOnAllDisksMount(async (fs) => {
			const path = '/1';
			await fs.chownAsync(path, 2000, 3000);
			const [stats] = await fs.statAsync(path);
			assert.strictEqual(stats.uid, 2000);
			assert.strictEqual(stats.gid, 3000);
		});
	});

	describe('lchown', () => {
		const target = '/1';
		const linkpath = '/testlink';
		testOnAllDisksMount(async (fs) => {
			await fs.symlinkAsync(target, linkpath);
			await fs.lchownAsync(linkpath, 5000, 6000);
			const [stats] = await fs.lstatAsync(linkpath);
			assert.strictEqual(stats.uid, 5000);
			assert.strictEqual(stats.gid, 6000);
		});
	});

	describe('O_EXCL', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.openAsync('/1', 'wx');
				assert(false);
			} catch(err) {
				assert.strictEqual(err.code, 'EEXIST');
				assert.strictEqual(err.errno, 20);
			}
		});
	});

	describe('O_DIRECTORY', () => {
		testOnAllDisksMount(async (fs) => {
			try {
				await fs.openAsync('/1', fs.constants.O_DIRECTORY);
				assert(false);
			} catch(err) {
				assert.strictEqual(err.code, 'ENOTDIR');
				assert.strictEqual(err.errno, 54);
			}
		});
	});

	describe('O_TRUNC', () => {
		testOnAllDisksMount(async (fs) => {
			const path = '/1';
			const [fd] = await fs.openAsync(path, 'w');
			await fs.closeAsync(fd);
			const [content] = await fs.readFileAsync(path, 'utf8');
			assert.strictEqual(content, '');
		});
	});

	describe('close all fds on umount', () => {
		testOnAllDisks(async (disk) => {
			const fs = Bluebird.promisifyAll((await ext2fs.mount(disk))[0], { multiArgs: true });
			await fs.openAsync('/1', 'r');
			await fs.openAsync('/2', 'r');
			await fs.openAsync('/3', 'r');
			// this is the function called before umount
			await fs.closeAllFileDescriptors();
			assert.strictEqual(fs.openFiles.size, 0);
			await ext2fs.umount(fs);
		});
	});

	describe('close bad fd', () => {
		testOnAllDisksMount(async (fs) => {
			const badFd = 9000;
			const buf = Buffer.alloc(8);
			let calls = [
				fs.closeAsync(badFd),
				fs.fchmodAsync(badFd, 0o777),
				fs.fchownAsync(badFd, 2000, 3000),
				fs.fstatAsync(badFd),
				fs.readAsync(badFd, buf, 0, buf.length, 0),
				fs.writeAsync(badFd, buf, 0, buf.length, 0)
			];
			calls = calls.map(async (p) => {
				try {
					await p;
					assert(false);
				} catch(err) {
					assert.strictEqual(err.code, 'EBADF');
					assert.strictEqual(err.errno, 8);
				}
			});
			await Promise.all(calls);
		});
	});

	describe('create 20 files at once', () => {
		testOnAllDisksMount(async (fs) => {
			const promises = [];
			for (let i=0; i<20; i++) {
				promises.push(fs.openAsync('/file_number_' + i, 'w'));
			}
			await Promise.all(promises);
		});
	});

	describe('createReadStream', () => {
		testOnAllDisksMount(async (fs) => {
			const contents = await readStream(fs.createReadStream('/1'));
			assert.strictEqual(contents, 'one\n');
		});
	});

	describe('createWriteStream', () => {
		testOnAllDisksMount(async (fs) => {
			const path = '/1';
			const newContent = 'wololo';
			const output = fs.createWriteStream(path);
			const input = createReadableStreamFromString(newContent);
			input.pipe(output);
			await waitStream(output);
			const [contents] = await fs.readFileAsync(path, 'utf8');
			assert.strictEqual(contents, newContent);
		});
	});

	describe('writeFile and readFile', () => {
		const filename = '/config.txt';
		const content = 'content\n';
		const encoding = 'utf8';
		testOnAllDisksMount(async (fs) => {
			await fs.writeFileAsync(filename, content, encoding);
			const [data] = await fs.readFileAsync(filename, encoding);
			assert.strictEqual(data, content);
		});
	});

	describe('trim', () => {
		testOnAllDisks(async (disk) => {
			const blockSize = 512;
			await ext2fs.withMountedDisk(disk, 0, async (fs) => {
				await fs.trim();
			});
			const ranges = await disk.getRanges(blockSize);
			if (disk.imageName === 'ext2') {
				assert.strictEqual(ranges.length, 3);
				assert.strictEqual(ranges[0].offset, 0);
				assert.strictEqual(ranges[0].length, 152576);
				assert.strictEqual(ranges[1].offset, 164864);
				assert.strictEqual(ranges[1].length, 2048);
				assert.strictEqual(ranges[2].offset, 3146752);
				assert.strictEqual(ranges[2].length, 5120);
			} else if (disk.imageName === 'ext3') {
				assert.strictEqual(ranges.length, 2);
				assert.strictEqual(ranges[0].offset, 0);
				assert.strictEqual(ranges[0].length, 1208320);
				assert.strictEqual(ranges[1].offset, 3146752);
				assert.strictEqual(ranges[1].length, 5120);
			} else if (disk.imageName === 'ext4') {
				assert.strictEqual(ranges.length, 2);
				assert.strictEqual(ranges[0].offset, 0);
				assert.strictEqual(ranges[0].length, 1208320);
				assert.strictEqual(ranges[1].offset, 3146752);
				assert.strictEqual(ranges[1].length, 5120);
			}
		});
	});

	describe('readlink', () => {
		const target = '/usr/bin/echo';
		const linkpath = '/testlink';

		testOnAllDisksMount(async (fs) => {
			await fs.symlinkAsync(target, linkpath);
			const [targetActual] = await fs.readlinkAsync(linkpath);

			assert.strictEqual(targetActual, target);
		});
	});

	describe('readlink non-existing', () => {
		const filename = '/testlink';

		testOnAllDisksMount(async (fs) => {
			await assert.rejects(async() => {
				await fs.readlinkAsync(filename);
			}, (err) => {
				return err.code === 'ENOENT';
			});
		});
	});

	describe('readlink non-link', () => {
		const filename = '/testlink';

		testOnAllDisksMount(async (fs) => {
			await fs.writeFileAsync(filename, 'Hello, World!');

			await assert.rejects(async() => {
				await fs.readlinkAsync(filename);
			}, (err) => {
				return err.code === 'EINVAL';
			});
		});
	});

	describe('file-handle open, read, close', () => {
		testOnAllDisksMount(async (_fs, fsPromises) => {
			const fh = await fsPromises.open('/1', 'r');
			const buf = Buffer.allocUnsafe(4);
			const {bytesRead, buffer} = await fh.read(buf, 0, 4, 0);
			assert.strictEqual(bytesRead, 4);
			assert.strictEqual(buffer.toString(), 'one\n');
			await fh.close();
		});
	});

	describe('writing to closed file-handle', () => {
		testOnAllDisksMount(async (fs, fsPromises) => {
			const fh = await fsPromises.open('/1', 'r');
			await fh.close();
			const str = 'hello, world';
			try {
				await fh.write(Buffer.from(str), 0, str.length);
			} catch (err) {
				assert.strictEqual(err.code, 'EBADF');
			}
		});
	});
});
