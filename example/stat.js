#!/usr/bin/env node
const argv = process.argv.slice(2);

if (argv.length !== 3) {
	return process.stderr.write('Usage: node example/stat.js <path-to-ext4_filesystem.img> <partition-offset> <file-to-stat>\n');
}

const fsFilename = argv.shift();
const fsOffset = Number(argv.shift());
const filename = argv.shift();

const ext2fs = require('..');
const filedisk = require('file-disk');
const fs = require('fs');
const util = require('util');

const fd = fs.openSync(fsFilename, 'r+');
const disk = new filedisk.FileDisk(fd);

ext2fs.mount(disk, { offset: fsOffset }, function(error, filesystem) {
	if (error) {
		return process.stderr.write(`Mount failed: ${error.stack}\n`);
	}
	process.stderr.write('Mounted filesystem successfully\n');
	filesystem.stat(filename, function(error, stats) {
		if (error) {
			return process.stderr.write(`Stat failed: ${error.stack}\n`);
		}
		process.stdout.write(`Stats: ${util.inspect(stats, { colors: process.stdout.isTTY })}\n`);
		ext2fs.umount(filesystem, function(error) {
			if (error) {
				process.stderr.write(`Unmount failed: ${error.stack}\n`);
			}
			fs.close(fd, function(error) {
				if (error) {
					process.stderr.write(`Close failed: ${error.stack}\n`);
				}
			});
		});
	});
});
