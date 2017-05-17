'use strict';

const filedisk = require('file-disk');

const constants = require('./constants');

class DiskWrapper extends filedisk.DiskWrapper {
	request(type, offset, length, buffer, callback) {
		switch(type) {
			case constants.E2FS_BLK_READ:
				this.disk.read(buffer, 0, buffer.length, offset, callback);
				break;
			case constants.E2FS_BLK_WRITE:
				this.disk.write(buffer, 0, buffer.length, offset, callback);
				break;
			case constants.E2FS_BLK_FLUSH:
				this.disk.flush(callback);
				break;
			case constants.E2FS_BLK_DISCARD:
				this.disk.discard(offset, length, callback);
				break;
			default:
				callback(new Error("Unknown request type: " + type));
		}
	}
}

exports.DiskWrapper = DiskWrapper;
