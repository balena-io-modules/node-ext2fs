const pathModule = require('path');
const Promise = require('bluebird');
const filedisk = require('resin-file-disk');

const ext2fs = Promise.promisifyAll(require('..'));

const EXT2_PATH = pathModule.join(__dirname, 'fixtures/ext2.img');
const EXT3_PATH = pathModule.join(__dirname, 'fixtures/ext3.img');
const EXT4_PATH = pathModule.join(__dirname, 'fixtures/ext4.img');

describe('ext2fs', function() {
	describe('mount', function() {
		it('ext2', function() {
			const disk = new filedisk.FileDisk(EXT2_PATH, ext2fs.constants.FILE_DISK_MAPPING);
			return ext2fs.mountAsync(disk);
		});
		it('ext3', function() {
			const disk = new filedisk.FileDisk(EXT3_PATH, ext2fs.constants.FILE_DISK_MAPPING);
			return ext2fs.mountAsync(disk);
		});
		it('ext4', function() {
			const disk = new filedisk.FileDisk(EXT4_PATH, ext2fs.constants.FILE_DISK_MAPPING);
			return ext2fs.mountAsync(disk);
		});
	});
	describe('trim', function() {
		it('ext4', function() {
			const disk = new filedisk.FileDisk(EXT4_PATH, ext2fs.constants.FILE_DISK_MAPPING);
			return ext2fs.mountAsync(disk)
			.then(function(fs) {
				return ext2fs.trimAsync(fs);
			});
		});
	});
});
