'use strict';

const toBuffer = require('typedarray-to-buffer');
const filedisk = require('file-disk');

const lib = require('./libext2fs')
const constants = require('./constants');

class DiskWrapper extends filedisk.DiskWrapper {
	request(type, offset, length, bufferPointer, lock) {
		let buffer
		if (bufferPointer !== 0) {
			buffer = toBuffer(lib.getBytes(bufferPointer, length));
		}
		switch(type) {
			case constants.E2FS_BLK_OPEN:
				lib.setValue(lock, 1, 'i32')
				break;
			case constants.E2FS_BLK_CLOSE:
				lib.setValue(lock, 1, 'i32')
				break;
			case constants.E2FS_BLK_READ:
				this.disk.read(buffer, 0, buffer.length, offset, function(err, count, bytes) {
					// TODO: handle errors
					lib.setValue(lock, 1, 'i32')
				});
				break;
			case constants.E2FS_BLK_WRITE:
				// TODO
				this.disk.write(buffer, 0, buffer.length, offset, callback);
				break;
			case constants.E2FS_BLK_FLUSH:
				// TODO
				this.disk.flush(callback);
				break;
			case constants.E2FS_BLK_DISCARD:
				this.disk.discard(offset, length, function(err) {
					// TODO: handle errors
					lib.setValue(lock, 1, 'i32')
				});
				break;
			default:
				console.log("Unknown request type: " + type)
				callback(new Error("Unknown request type: " + type));
		}
	}
}

exports.DiskWrapper = DiskWrapper;
