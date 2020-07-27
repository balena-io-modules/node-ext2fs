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

	request(type, offset, length, buffer, fn, s, callback2) {
		const callback = (err) => {
			callback2(err, fn, s);
		};
		offset += this.offset;
		switch(type) {
			case constants.E2FS_BLK_OPEN:
			case constants.E2FS_BLK_CLOSE:
			case constants.E2FS_BLK_CACHE_READAHEAD:
				callback(null);
				break;
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
			case constants.E2FS_BLK_ZEROOUT:
				// Writes zeroes then discards, TODO: test case
				promiseToCallback(
					this.disk.write(Buffer.alloc(length), 0, length, offset)
						.then(() => this.disk.discard(offset, length)),
					callback
				);
				break;
			default:
				// eslint-disable-next-line no-console
				console.warn('Unknown request type in DiskWrapper.request():', type);
				callback(constants.EINVAL);  // js_request_done expects null or an integer as error.
		}
	}
}

exports.DiskWrapper = DiskWrapper;
