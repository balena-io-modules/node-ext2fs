'use strict';

const constants = require('./constants');

const promiseToCallback = (promise, callback, transform) => {
	promise
	.then((result) => {
		const args = (transform === undefined) ? [] : transform(result);
		callback(null, ...args);
	})
	.catch(callback);
};

class DiskWrapper {
	constructor(disk, offset=0) {
		this.disk = disk;
		this.offset = offset;
	}

	request(type, offset, length, buffer, callback) {
		offset += this.offset;
		switch(type) {
			case constants.E2FS_BLK_READ:
				promiseToCallback(
					this.disk.read(buffer, 0, buffer.length, offset),
					callback,
					({ bytesRead, buffer }) => [ bytesRead, buffer ]
				);
				break;
			case constants.E2FS_BLK_WRITE:
				promiseToCallback(
					this.disk.write(buffer, 0, buffer.length, offset),
					callback,
					({ bytesWritten, buffer }) => [ bytesWritten, buffer ]
				);
				break;
			case constants.E2FS_BLK_FLUSH:
				promiseToCallback(this.disk.flush(), callback);
				break;
			case constants.E2FS_BLK_DISCARD:
				promiseToCallback(this.disk.discard(offset, length), callback);
				break;
			default:
				callback(new Error('Unknown request type: ' + type));
		}
	}
}

exports.DiskWrapper = DiskWrapper;
