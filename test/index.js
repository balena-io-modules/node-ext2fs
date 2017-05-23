/*global it describe after*/
/*eslint no-undef: "error"*/

const assert = require('assert');
const pathModule = require('path');
const Promise = require('bluebird');
const filedisk = require('file-disk');

const ext2fs = Promise.promisifyAll(require('..'));

const IMAGES = {
	'ext2': 'ext2.img',
	'ext3': 'ext3.img',
	'ext4': 'ext4.img'
};

function testOnAllDisks(fn) {
	return Object.keys(IMAGES).forEach(function(name) {
		it(name, function(){
			const path = pathModule.join(__dirname, 'fixtures', IMAGES[name]);
			return Promise.using(filedisk.openFile(path, 'r'), function(fd) {
				const options = { readOnly: true, recordWrites: true };
				const disk = new filedisk.FileDisk(fd, options);
				return fn(disk);
			});
		});
	});
}

describe('ext2fs', function() {
	after(ext2fs.close);

	describe('mount, open, read, close, umount', function() {
		testOnAllDisks(function(disk) {
			return ext2fs.mountAsync(disk)
			.then(function(fs){
				fs = Promise.promisifyAll(fs, { multiArgs: true });
				const buf = Buffer.allocUnsafe(4);
				return fs.openAsync('/1', 'r')
				.then(function(fd) {
					return fs.readAsync(fd, buf, 0, 4, 0)
					.spread(function(bytesRead, buf) {
						assert.strictEqual(bytesRead, 4);
						assert.strictEqual(buf.toString(), 'one\n');
						return fs.closeAsync(fd);
					});
				})
				.then(function() {
					return ext2fs.umountAsync(fs);
				});
			});
		});
	});

	describe('mount, create, write, read, close, umount', function() {
		testOnAllDisks(function(disk) {
			return ext2fs.mountAsync(disk)
			.then(function(fs){
				fs = Promise.promisifyAll(fs, { multiArgs: true });
				const buf = Buffer.from('six\n');
				return fs.openAsync('/6', 'r+')
				.then(function(fd) {
					return fs.writeAsync(fd, buf, 0, buf.length, 0)
					.spread(function(bytesWritten, buf) {
						assert.strictEqual(bytesWritten, buf.length);
						assert.strictEqual(buf.toString(), 'six\n');
						buf.fill(0);
						return fs.readAsync(fd, buf, 0, 1024, 0);
					})
					.spread(function(bytesRead, buf) {
						assert.strictEqual(bytesRead, 4);
						assert.strictEqual(buf.toString(), 'six\n');
						return fs.closeAsync(fd);
					});
				})
				.then(function() {
					return ext2fs.umountAsync(fs);
				});
			});
		});
	});

	describe('mount, readFile, umount', function() {
		testOnAllDisks(function(disk) {
			return ext2fs.mountAsync(disk)
			.then(function(fs){
				fs = Promise.promisifyAll(fs, { multiArgs: true });
				return fs.readFileAsync('/1', 'utf8')
				.spread(function(data) {
					assert.strictEqual(data, 'one\n');
					return ext2fs.umountAsync(fs);
				});
			});
		});
	});

	describe('mount, readdir, umount', function() {
		testOnAllDisks(function(disk) {
			return ext2fs.mountAsync(disk)
			.then(function(fs){
				fs = Promise.promisifyAll(fs, { multiArgs: true });
				return fs.readdirAsync('/')
				.spread(function(filenames) {
					filenames.sort();
					assert.deepEqual(
						filenames,
						[ '1', '2', '3', '4', '5', 'lost+found' ]
					);
					return ext2fs.umountAsync(fs);
				});
			});
		});
	});

	describe('trim', function() {
		testOnAllDisks(function(disk) {
			return ext2fs.mountAsync(disk)
			.then(function(fs) {
				return ext2fs.trimAsync(fs)
				.then(function(){
					return ext2fs.umountAsync(fs);
				});
			});
		});
	});
});
