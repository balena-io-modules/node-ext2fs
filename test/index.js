const pathModule = require('path');
const Promise = require('bluebird');
const filedisk = require('file-disk');

const ext2fs = Promise.promisifyAll(require('..'));

const EXT2_PATH = pathModule.join(__dirname, 'fixtures/ext2.img');
const EXT3_PATH = pathModule.join(__dirname, 'fixtures/ext3.img');
const EXT4_PATH = pathModule.join(__dirname, 'fixtures/ext4.img');

describe('ext2fs', function() {
	describe('mount', function() {
		it('ext2', function() {
			return Promise.using(filedisk.openFile(EXT2_PATH, 'r'), function(fd) {
				return ext2fs.mountAsync(new filedisk.FileDisk(fd))
				.then(function(filesystem) {
					return ext2fs.umountAsync(filesystem);
				});
			})
		});
		it('ext3', function() {
			return Promise.using(filedisk.openFile(EXT3_PATH, 'r'), function(fd) {
				return ext2fs.mountAsync(new filedisk.FileDisk(fd))
				.then(function(filesystem) {
					return ext2fs.umountAsync(filesystem);
				});
			});
		});
		it('ext4', function() {
			return Promise.using(filedisk.openFile(EXT4_PATH, 'r'), function(fd) {
				return ext2fs.mountAsync(new filedisk.FileDisk(fd))
				.then(function(filesystem) {
					return ext2fs.umountAsync(filesystem);
				});
			})
		});
	});
	describe('trim', function() {
		it('ext4', function() {
			return Promise.using(filedisk.openFile(EXT4_PATH, 'r'), function(fd) {
				return ext2fs.mountAsync(new filedisk.FileDisk(fd));
			})
			.then(function(filesystem) {
				return ext2fs.trimAsync(filesystem)
				.then(function() {
					return ext2fs.umountAsync(filesystem);
				});
			});
		});
	});
});
