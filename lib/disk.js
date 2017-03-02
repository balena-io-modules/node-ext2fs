const EventEmitter = require('events');
const fs = require('fs');

const constants = require('./constants');
class FileDisk extends EventEmitter { constructor(path) {
		super();
		this.path = path;
		this.fd = null;
	}

	request(type, offset, length, buffer, callback) {
		switch (type) {
			case constants.E2FS_BLK_OPEN:
				fs.open(this.path, 'r+', (err, fd) => {
					if (err) {
						return callback(err);
					}
					this.fd = fd;
					callback(null);
				});
				break;
			case constants.E2FS_BLK_READ:
				fs.read(this.fd, buffer, 0, buffer.length, offset, callback);
				break;
			case constants.E2FS_BLK_FLUSH:
				fs.fdatasync(this.fd, callback);
				break;
			case constants.E2FS_BLK_DISCARD:
				break;
			default:
				throw new Error("Unknown request type: " + type);
		}
	};
}

exports.FileDisk = FileDisk;
