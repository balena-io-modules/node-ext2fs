const pathModule = require('path');
const Promise = require('bluebird');
const ext2fs = Promise.promisifyAll(require('..'));

EXT2_PATH = pathModule.join(__dirname, 'fixtures/ext2.img');
EXT3_PATH = pathModule.join(__dirname, 'fixtures/ext3.img');
EXT4_PATH = pathModule.join(__dirname, 'fixtures/ext4.img');

describe('ext2fs', function() {
	describe('mount', function() {
		it('ext2', function() {
			const disk = new ext2fs.disk.FileDisk(EXT2_PATH);
			return ext2fs.mountAsync(disk);
		});
		it('ext3', function() {
			const disk = new ext2fs.disk.FileDisk(EXT3_PATH);
			return ext2fs.mountAsync(disk);
		});
		it('ext4', function() {
			const disk = new ext2fs.disk.FileDisk(EXT4_PATH);
			return ext2fs.mountAsync(disk);
		});
	});
	describe('trim', function() {
		it('ext4', function() {
			const disk = new ext2fs.disk.FileDisk(EXT4_PATH);
			return ext2fs.mountAsync(disk)
			.then(function(fs) {
				return ext2fs.trimAsync(fs);
			});
		});
	});
});
