/*global it describe after*/
/*eslint no-undef: "error"*/

const assert = require('assert');
const pathModule = require('path');
const Promise = require('bluebird');
const filedisk = require('file-disk');

const ext2fs = Promise.promisifyAll(require('..'));

// Each image contains 6 files named 1, 2, 3, 4, 5 and containing
// 'one\n', 'two\n', 'three\n', 'four\n', 'five\n' respectively.

const IMAGES = {
	'ext2': 'ext2.img',
	'ext3': 'ext3.img',
	'ext4': 'ext4.img'
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
			const path = pathModule.join(__dirname, 'fixtures', IMAGES[name]);
			return Promise.using(filedisk.openFile(path, 'r'), function(fd) {
				return fn(new filedisk.FileDisk(fd, true, true));
			});
		});
	});
}

function testOnAllDisksMount(fn) {
	return testOnAllDisks(function(disk) {
		return ext2fs.mountAsync(disk)
		.then(function(fs) {
			fs = Promise.promisifyAll(fs, { multiArgs: true });
			return fn(fs)
			.then(function() {
				return ext2fs.umountAsync(fs);
			});
		});
	});
}

describe('ext2fs', function() {
	after(ext2fs.close);

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
				assert.strictEqual(stats.blksize, 1024);
				assert.strictEqual(stats.size, 4);
				assert.strictEqual(stats.blocks, 2);
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
			return fs.openAsync('/2', fs.O_RDONLY | fs.O_NOATIME)  // TODO: check
			.spread(function(fd) {
				return fs.fstatAsync(fd)
				.spread(function(stats) {
					assert.strictEqual(stats.dev, 0);
					assert.strictEqual(stats.mode, 33188);
					assert.strictEqual(stats.nlink, 1);
					assert.strictEqual(stats.uid, 1000);
					assert.strictEqual(stats.gid, 1000);
					assert.strictEqual(stats.rdev, 0);
					assert.strictEqual(stats.blksize, 1024);
					assert.strictEqual(stats.size, 4);
					assert.strictEqual(stats.blocks, 2);
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
				})
			});
		});
	});

	describe('mount, open, write, fstat, close, umount', function() {
		testOnAllDisksMount(function(fs) {
			const buf = Buffer.from('hello');
			return fs.openAsync('/2', 'w')
			.spread(function(fd) {
				return fs.writeAsync(fd, buf, 0, buf.length, 0)
				.spread(function(bytesWritten, buf) {
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					// ctime, mtime and birthtime should change
					const now = Date.now();
					assert(now - stats.ctime.getTime() < 3000);
					assert(now - stats.mtime.getTime() < 3000);
					assert(now - stats.birthtime.getTime() < 3000);
					return fs.closeAsync(fd);
				});
			})
			.then(function() {
				return fs.openAsync('/2', 'r')
				.spread(function(fd) {
					return fs.readAsync(fd, buf, 0, buf.length, 0)
					.spread(function(bytesRead, buf) {
						return fs.fstatAsync(fd)
					})
					.spread(function(stats) {
						assert(Date.now() - stats.atime.getTime() < 1000);
						return fs.closeAsync(fd);
					});
				});
			});
		});
	});

	describe('mount, open, write string, read, close, umount', function() {
		testOnAllDisksMount(function(fs) {
			const string = 'hello';
			return fs.openAsync('/9', 'w')
			.spread(function(fd) {
				return fs.writeAsync(fd, string)
				.spread(function(bytesWritten, s) {
					assert.strictEqual(bytesWritten, string.length);
					assert.strictEqual(s, string);
					return fs.fstatAsync(fd);
				})
				.spread(function(stats) {
					return fs.closeAsync(fd);
				});
			})
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
						value = statsBeforeClose[key]
						otherValue = stats[key]
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
					assert.strictEqual(stats.blocks, 2);
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
				})
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
			.spread(function(fd) {
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
			.spread(function(fd) {
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
				return fs.readdirAsync('/')
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
				assert.strictEqual(error.code, 'ENOENT')
				assert.strictEqual(error.errno, 2)
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
				return fs.unlinkAsync(Buffer.from('/2'))
			})
			.then(function() {
				return fs.readdirAsync('/')
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
				assert.strictEqual(error.code, 'ENOENT')
				assert.strictEqual(error.errno, 2)
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

	describe('trim', function() {
		testOnAllDisksMount(ext2fs.trimAsync);
	});
});
