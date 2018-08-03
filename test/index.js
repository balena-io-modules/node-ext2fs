'use strict';
/*global it describe*/
/*eslint no-undef: "error"*/

const assert = require('assert');
const pathModule = require('path');
const Promise = require('bluebird');
const filedisk = Promise.promisifyAll(require('file-disk'));
const stream = require('stream');

const ext2fs = Promise.promisifyAll(require('..'));

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
	return Object.keys(IMAGES).forEach(function(name) {
		it(name, function(){
			this.timeout(5000);
			const path = pathModule.join(__dirname, 'fixtures', IMAGES[name]);
			return Promise.using(filedisk.openFile(path, 'r'), function(fd) {
				const disk = new filedisk.FileDisk(fd, true, true);
				// `disk.imageName` will be useful in tests that have different
				// results depending on the image.
				disk.imageName = name;
				return fn(disk);
			});
		});
	});
}

function testOnAllDisksMount(fn) {
	return testOnAllDisks(function(disk) {
		return Promise.using(ext2fs.mountDisposer(disk), function(fs) {
			// Might be useful to get the disk name
			fs.disk = disk;
			return fn(Promise.promisifyAll(fs, { multiArgs: true }));
		});
	});
}

function readStream(stream) {
	return new Promise(function(resolve, reject) {
		const chunks = [];
		stream.on('error', reject);
		stream.on('close', function(){
			resolve(chunks.join(''));
		});
		stream.on('data', function(chunk) {
			chunks.push(chunk);
		});
	});
}

function waitStream(stream) {
	return new Promise(function(resolve, reject) {
		stream.on('error', reject);
		stream.on('close', resolve);
	});
}

function createReadableStreamFromString(s) {
	const readable = new stream.Readable();
	readable._read = function () {};
	readable.push(s);
	readable.push(null);
	return readable;
}

describe('ext2fs', function() {
	describe('mount, open, read, close, umount', function() {
		testOnAllDisksMount(function(fs) {
			const buf = Buffer.allocUnsafe(4);
			return fs.openAsync('/1', 'r')
			.spread(function(fd) {
				return fs.readAsync(fd, buf, 0, 4, 0)
				.spread(function(bytesRead, buf) {
					assert.strictEqual(bytesRead, 4);
					assert.strictEqual(buf.toString(), 'one\n');
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('mount, stat, umount', function() {
		testOnAllDisksMount(function(fs) {
			return fs.statAsync('/2')
			.spread(function(stats) {
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
	});

	describe('mount, open, fstat, close, umount', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/2', fs.constants.O_RDONLY | fs.constants.O_NOATIME)
			.spread(function(fd) {
				return fs.fstatAsync(fd)
				.spread(function(stats) {
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
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('mount, open, write, fstat, close, open, read, fstat, close, umount', function() {
		testOnAllDisksMount(function(fs) {
			const string = 'hello';
			const buf = Buffer.from(string);
			return fs.openAsync('/2', 'w')
			.spread(function(fd) {
				return fs.writeAsync(fd, buf, 0, buf.length, 0)
				.then(function() {
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					// ctime, mtime and birthtime should change
					const now = Date.now();
					assert(now - stats.ctime.getTime() < 3000);
					assert(now - stats.mtime.getTime() < 3000);
					assert(now - stats.birthtime.getTime() < 3000);
					assert.strictEqual(stats.size, 5);
					return fs.closeAsync(fd);
				});
			})
			.then(function() {
				return fs.openAsync('/2', 'r')
				.spread(function(fd) {
					return fs.readAsync(fd, buf, 0, buf.length, 0)
					.spread(function(bytesRead, buf) {
						assert.strictEqual(buf.toString(), string);
						return fs.fstatAsync(fd);
					})
					.spread(function(stats) {
						assert(Date.now() - stats.atime.getTime() < 1000);
						assert.strictEqual(stats.size, 5);
						return fs.closeAsync(fd);
					});
				});
			});
		});
	});

	describe('mount, open, write string, read, close, umount', function() {
		testOnAllDisksMount(function(fs) {
			const string = 'hello';
			const buffer = Buffer.alloc(string.length);
			return fs.openAsync('/9', 'w+')
			.spread(function(fd) {
				return fs.writeAsync(fd, string)
				.spread(function(bytesWritten, s) {
					assert.strictEqual(bytesWritten, string.length);
					assert.strictEqual(s, string);
					return fs.readAsync(fd, buffer, 0, buffer.length, 0);
				})
				.spread(function(bytesRead, buf) {
					assert.strictEqual(buf.toString(), string);
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('mount, create, write, fstat, close, open, fstat, read, close, umount', function() {
		const path = '/6';
		const content = 'six\n';
		testOnAllDisksMount(function(fs) {
			let statsBeforeClose;
			const buf = Buffer.from(content);
			return fs.openAsync(path, 'w+', 0o777)
			.spread(function(fd) {
				return fs.writeAsync(fd, buf, 0, buf.length, 0)
				.spread(function(bytesWritten, buf) {
					assert.strictEqual(bytesWritten, buf.length);
					assert.strictEqual(buf.toString(), content);
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					statsBeforeClose = stats;
					return fs.closeAsync(fd);
				});
			})
			.then(function() {
				return fs.openAsync(path, 'r');
			})
			.spread(function(fd) {
				return fs.fstatAsync(fd)
				.spread(function(stats) {
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
					return fs.readAsync(fd, buf, 0, 1024, 0);
				})
				.spread(function(bytesRead, buf) {
					assert.strictEqual(bytesRead, content.length);
					assert.strictEqual(buf.toString(), content);
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('mount, readFile, umount', function() {
		testOnAllDisksMount(function(fs) {
			return fs.readFileAsync('/1', 'utf8')
			.spread(function(data) {
				assert.strictEqual(data, 'one\n');
			});
		});
	});

	describe('mount, readdir, umount', function() {
		testOnAllDisksMount(function(fs) {
			return fs.readdirAsync('/')
			.spread(function(filenames) {
				filenames.sort();
				assert.deepEqual(
					filenames,
					[ '1', '2', '3', '4', '5', 'lost+found' ]
				);
			});
		});
	});

	describe('mount, create, write 1M, read 1M, close, umount', function() {
		testOnAllDisksMount(function(fs) {
			const size = Math.pow(1024, 2);
			const buf = Buffer.allocUnsafe(size);
			buf.fill(1);
			return fs.openAsync('/8', 'w+')
			.spread(function(fd) {
				return fs.writeAsync(fd, buf, 0, size, 0)
				.spread(function(bytesWritten, buf) {
					assert.strictEqual(bytesWritten, size);
					buf.fill(0);
					return fs.readAsync(fd, buf, 0, size, 0);
				})
				.spread(function(bytesRead, buf) {
					const buf2 = Buffer.allocUnsafe(size);
					buf2.fill(1);
					assert.strictEqual(bytesRead, size);
					assert(buf.equals(buf2));
					return fs.closeAsync(fd);
				});
			});
		});
	});


	describe('open non existent file for reading', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/7', 'r')
			.then(function() {
				assert(false);
			})
			.catch(function(err) {
				assert.strictEqual(err.errno, 2);
				assert.strictEqual(err.code, 'ENOENT');
			});
		});
	});

	describe('create file in non existent folder', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/7/8', 'w')
			.then(function() {
				assert(false);
			})
			.catch(function(err) {
				assert.strictEqual(err.errno, 20);
				assert.strictEqual(err.code, 'ENOTDIR');
			});
		});
	});

	describe('rmdir', function() {
		testOnAllDisksMount(function(fs) {
			return fs.rmdirAsync('/lost+found')
			.then(function() {
				return fs.readdirAsync('/');
			})
			.spread(function(files) {
				files.sort();
				assert.deepEqual(files, [ '1', '2', '3', '4', '5' ]);
			});
		});
	});

	describe('rmdir a folder that does not exist', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.rmdirAsync('/no-such-folder')
			.catch(function(err) {
				error = err;
			})
			.then(function() {
				assert.strictEqual(error.code, 'ENOENT');
				assert.strictEqual(error.errno, 2);
			});
		});
	});

	describe('rmdir a file', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.rmdirAsync('/1')
			.catch(function(err) {
				error = err;
			})
			.then(function() {
				assert.strictEqual(error.code, 'ENOTDIR');
				assert.strictEqual(error.errno, 20);
			});
		});
	});

	describe('unlink', function() {
		testOnAllDisksMount(function(fs) {
			return fs.unlinkAsync('/1')
			.then(function() {
				return fs.unlinkAsync(Buffer.from('/2'));
			})
			.then(function() {
				return fs.readdirAsync('/');
			})
			.spread(function(files) {
				files.sort();
				assert.deepEqual(files, [ '3', '4', '5', 'lost+found' ]);
			});
		});
	});

	describe('unlink a file that does not exist', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.unlinkAsync('/no-such-file')
			.catch(function(err) {
				error = err;
			})
			.then(function() {
				assert.strictEqual(error.code, 'ENOENT');
				assert.strictEqual(error.errno, 2);
			});
		});
	});

	describe('unlink a directory', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.unlinkAsync('/lost+found')
			.catch(function(err) {
				error = err;
			})
			.then(function() {
				assert.strictEqual(error.code, 'EISDIR');
				assert.strictEqual(error.errno, 21);
			});
		});
	});

	describe('access', function() {
		testOnAllDisksMount(function(fs) {
			return fs.accessAsync('/1');
		});
	});

	describe('execute access on a file that can not be executed', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.accessAsync('/1', fs.constants.X_OK)
			.catch(function(err) {
				error = err;
			})
			.then(function() {
				assert.strictEqual(error.code, 'EACCES');
				assert.strictEqual(error.errno, 13);
			});
		});
	});

	describe('mkdir', function() {
		testOnAllDisksMount(function(fs) {
			return fs.mkdirAsync('/new-folder')
			.then(function() {
				return fs.readdirAsync('/');
			})
			.spread(function(files) {
				files.sort();
				assert.deepEqual(
					files,
					[ '1', '2', '3', '4', '5', 'lost+found', 'new-folder' ]
				);
			});
		});
	});

	describe('mkdir specific mode', function() {
		testOnAllDisksMount(function(fs) {
			return fs.mkdirAsync('/new-folder', 0o467)
			.then(function() {
				return fs.readdirAsync('/');
			})
			.spread(function(files) {
				files.sort();
				assert.deepEqual(
					files,
					[ '1', '2', '3', '4', '5', 'lost+found', 'new-folder' ]
				);
				return fs.statAsync('/new-folder');
			})
			.spread(function(stats) {
				assert.strictEqual(humanFileMode(fs, stats), 'dr--rw-rwx');
			});
		});
	});

	describe('write in a readonly file', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.openAsync('/1', 'r')
			.spread(function(fd) {
				return fs.writeAsync(fd, 'two')
				.catch(function(err) {
					error = err;
				})
				.then(function() {
					assert.strictEqual(error.errno, 9);
					assert.strictEqual(error.code, 'EBADF');
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('read a writeonly file', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.openAsync('/1', 'w')
			.spread(function(fd) {
				return fs.readFileAsync(fd, 'utf8')
				.catch(function(err) {
					error = err;
				})
				.then(function() {
					assert.strictEqual(error.errno, 9);
					assert.strictEqual(error.code, 'EBADF');
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('append mode', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/1', 'a+')
			.spread(function(fd) {
				const text = 'two\n';
				return fs.writeAsync(fd, text)
				.spread(function(bytesWritten, data) {
					assert.strictEqual(bytesWritten, text.length);
					assert.strictEqual(data, text);
					const buffer = Buffer.alloc(16);
					return fs.readAsync(fd, buffer, 0, buffer.length, 0);
				})
				.spread(function(bytesRead, data) {
					assert.strictEqual(bytesRead, 8);
					const dataStr = data.slice(0, bytesRead).toString();
					assert.strictEqual(dataStr, 'one\ntwo\n');
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('readdir a folder that does not exist', function() {
		testOnAllDisksMount(function(fs) {
			let error = null;
			return fs.readdirAsync('/no-such-folder')
			.catch(function(err) {
				error = err;
			})
			.then(function() {
				assert.strictEqual(error.errno, 2);
				assert.strictEqual(error.code, 'ENOENT');
			});
		});
	});

	describe('fchmod', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/1', 'r')
			.spread(function(fd) {
				return fs.fchmodAsync(fd, 0o777)
				.then(function() {
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					assert.strictEqual(humanFileMode(fs, stats), '-rwxrwxrwx');
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('fchmod2', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/1', 'r')
			.spread(function(fd) {
				return fs.fchmodAsync(fd, 0o137)
				.then(function() {
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					assert.strictEqual(humanFileMode(fs, stats), '---x-wxrwx');
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('fchmod a folder', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/lost+found', 'r')
			.spread(function(fd) {
				return fs.fchmodAsync(fd, 0o137)
				.then(function() {
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					assert.strictEqual(humanFileMode(fs, stats), 'd--x-wxrwx');
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('chmod', function() {
		testOnAllDisksMount(function(fs) {
			const path = '/1';
			return fs.chmodAsync(path, 0o777)
			.then(function() {
				return fs.statAsync(path);
			})
			.spread(function(stats) {
				assert.strictEqual(humanFileMode(fs, stats), '-rwxrwxrwx');
			});
		});
	});

	describe('chmod 2', function() {
		testOnAllDisksMount(function(fs) {
			const path = '/1';
			return fs.chmodAsync(path, 0o137)
			.then(function() {
				return fs.statAsync(path);
			})
			.spread(function(stats) {
				assert.strictEqual(humanFileMode(fs, stats), '---x-wxrwx');
			});
		});
	});

	describe('chmod a folder', function() {
		testOnAllDisksMount(function(fs) {
			const path = '/lost+found';
			return fs.chmodAsync(path, 0o137)
			.then(function() {
				return fs.statAsync(path);
			})
			.spread(function(stats) {
				assert.strictEqual(humanFileMode(fs, stats), 'd--x-wxrwx');
			});
		});
	});

	describe('fchown', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/1', 'r')
			.spread(function(fd) {
				return fs.fchownAsync(fd, 2000, 3000)
				.then(function() {
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					assert.strictEqual(stats.uid, 2000);
					assert.strictEqual(stats.gid, 3000);
					return fs.closeAsync(fd);
				});
			});
		});
	});

	describe('chown', function() {
		testOnAllDisksMount(function(fs) {
			const path = '/1';
			return fs.chownAsync(path, 2000, 3000)
			.then(function() {
				return fs.statAsync(path);
			})
			.spread(function(stats) {
				assert.strictEqual(stats.uid, 2000);
				assert.strictEqual(stats.gid, 3000);
			});
		});
	});

	describe('O_EXCL', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/1', 'wx')
			.then(function() {
				assert(false);
			})
			.catch(function(err) {
				assert.strictEqual(err.code, 'EEXIST');
				assert.strictEqual(err.errno, 17);
			});
		});
	});

	describe('O_DIRECTORY', function() {
		testOnAllDisksMount(function(fs) {
			return fs.openAsync('/1', fs.constants.O_DIRECTORY)
			.then(function() {
				assert(false);
			})
			.catch(function(err) {
				assert.strictEqual(err.code, 'ENOTDIR');
				assert.strictEqual(err.errno, 20);
			});
		});
	});

	describe('O_TRUNC', function() {
		testOnAllDisksMount(function(fs) {
			const path = '/1';
			return fs.openAsync(path, 'w')
			.spread(function(fd) {
				return fs.closeAsync(fd);
			})
			.then(function() {
				return fs.readFileAsync(path, 'utf8');
			})
			.spread(function(content) {
				assert.strictEqual(content, '');
			});
		});
	});

	describe('close all fds on umount', function() {
		testOnAllDisks(function(disk) {
			return ext2fs.mountAsync(disk)
			.then(function(fs) {
				fs = Promise.promisifyAll(fs, { multiArgs: true });
				return fs.openAsync('/1', 'r')
				.then(function() {
					return fs.openAsync('/2', 'r');
				})
				.then(function() {
					return fs.openAsync('/3', 'r');
				})
				.then(function() {
					// this is the function called before umount
					return fs.closeAllFileDescriptorsAsync();
				})
				.then(function() {
					return fs.openAsync('/1', 'r');
				})
				.spread(function(fd) {
					// if all fds were closed, the next fd should be 0
					assert.strictEqual(fd, 0);
					return ext2fs.umountAsync(fs);
				});
			});
		});
	});

	describe('close bad fd', function() {
		testOnAllDisksMount(function(fs) {
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
			calls = calls.map(function(p) {
				return p
				.then(function() {
					assert(false);
				})
				.catch(function(err) {
					assert.strictEqual(err.code, 'EBADF');
					assert.strictEqual(err.errno, 9);
				});
			});
			return Promise.all(calls);
		});
	});

	function openFile(fs, path, mode) {
		return fs.openAsync(path, mode)
		.disposer(function(fd) {
			return fs.closeAsync(fd);
		});
	}

	describe('MAX_FD', function() {
		testOnAllDisks(function(disk) {
			const disposer = ext2fs.mountDisposer(disk, { MAX_FD: 2 });
			return Promise.using(disposer, function(fs) {
				fs = Promise.promisifyAll(fs);
				const files = [
					openFile(fs, '/1', 'r'),
					openFile(fs, '/2', 'r')
				];
				return Promise.using(files, function() {
					return fs.openAsync('/3', 'r')
					.then(function() {
						assert(false);
					})
					.catch(function(err) {
						assert.strictEqual(err.code, 'EMFILE');
						assert.strictEqual(err.errno, 24);
					});
				});
			});
		});
	});

	describe('create 20 files at once', function() {
		testOnAllDisksMount(function(fs) {
			const promises = [];
			for (let i=0; i<20; i++) {
				promises.push(fs.openAsync('/file_number_' + i, 'w'));
			}
			return Promise.all(promises);
		});
	});

	describe('createReadStream', function() {
		testOnAllDisksMount(function(fs) {
			return readStream(fs.createReadStream('/1'))
			.then(function(contents) {
				assert.strictEqual(contents, 'one\n');
			});
		});
	});

	describe('createWriteStream', function() {
		testOnAllDisksMount(function(fs) {
			const path = '/1';
			const newContent = 'wololo';
			const output = fs.createWriteStream(path);
			const input = createReadableStreamFromString(newContent);
			input.pipe(output);
			return waitStream(output)
			.then(function() {
				return fs.readFileAsync(path, 'utf8');
			})
			.spread(function(contents) {
				assert.strictEqual(contents, newContent);
			});
		});
	});

	describe('writeFile and readFile with no slash at the beginning', function() {
		const filename = 'config.txt';
		const content = 'content\n';
		const encoding = 'utf8';
		testOnAllDisksMount(function(fs) {
			return fs.writeFileAsync(filename, content, encoding)
			.then(function() {
				return fs.readFileAsync(filename, encoding);
			})
			.spread(function(data) {
				assert.strictEqual(data, content);
			});
		});
	});

	describe('trim', function() {
		testOnAllDisks(function(disk) {
			const blockSize = 512;
			return Promise.using(ext2fs.mountDisposer(disk), function(fs) {
				fs = Promise.promisifyAll(fs);
				return fs.trimAsync();
			})
			.then(function() {
				return disk.getBlockMap(blockSize, true);  // true is for calculateChecksums
			})
			.then(function(bmap) {
				assert.strictEqual(bmap.imageSize, 4194304);
				assert.strictEqual(bmap.blockSize, blockSize);
				assert.strictEqual(bmap.blockCount, bmap.imageSize / blockSize);
				if (disk.imageName === 'ext2') {
					assert.strictEqual(
						bmap.checksum,
						'a901df1b0db55e5b98cc313a136ea945da60a5738ffcb8aee4111ee8ea12d08f'
					);
					assert.strictEqual(bmap.mappedBlockCount, 312);
					assert.strictEqual(bmap.ranges.length, 3);
					assert.strictEqual(
						bmap.ranges[0].checksum,
						'54bfcc09dbcdcd97e35b072c2056f0c0f9bc9cfbec01e0735236aea2266424de'
					);
					assert.strictEqual(
						bmap.ranges[1].checksum,
						'3c43be3292a0c5103237a3bb800b42ce45339825355df51776c868ff8ff452ed'
					);
					assert.strictEqual(
						bmap.ranges[2].checksum,
						'6d89b59b2dfe850352354da6b2a400d2960f08844aece84735a1c5ed990b4b85'
					);
				} else if (disk.imageName === 'ext3') {
					assert.strictEqual(
						bmap.checksum,
						'2cdc6d68929de06e7786e046ca503434f1c4ef0009aea0fafd73e08e39293c92'
					);
					assert.strictEqual(bmap.mappedBlockCount, 2370);
					assert.strictEqual(bmap.ranges.length, 2);
					assert.strictEqual(
						bmap.ranges[0].checksum,
						'36e8cd5f6f193af17acb34807e89c5b00407bbc13a52c8177b5af2ef536a2929'
					);
					assert.strictEqual(
						bmap.ranges[1].checksum,
						'6d89b59b2dfe850352354da6b2a400d2960f08844aece84735a1c5ed990b4b85'
					);
				} else if (disk.imageName === 'ext4') {
					assert.strictEqual(
						bmap.checksum,
						'd963ae8a7cfbce425e9450fc8f18fadda24d7328b88de3fd821557be7bbf6918'
					);
					assert.strictEqual(bmap.mappedBlockCount, 2370);
					assert.strictEqual(bmap.ranges.length, 2);
					assert.strictEqual(
						bmap.ranges[0].checksum,
						'3997d2475785686abaaf122463224d167df1b54341cfe69e63f2eece1b9c7d1c'
					);
					assert.strictEqual(
						bmap.ranges[1].checksum,
						'6d89b59b2dfe850352354da6b2a400d2960f08844aece84735a1c5ed990b4b85'
					);
				}
			});
		});
	});
});
