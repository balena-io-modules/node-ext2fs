'use strict';

class DiskWrapper {
	constructor(disk, offset=0) {
		this.disk = disk;
		this.offset = offset;
	}

	async read(buffer, bufferOffset, length, fileOffset) {
		return await this.disk.read(buffer, bufferOffset, length, fileOffset + this.offset);
	}

	async write(buffer, bufferOffset, length, fileOffset) {
		return await this.disk.write(buffer, bufferOffset, length, fileOffset + this.offset);
	}

	async discard(offset, length) {
		return await this.disk.discard(offset + this.offset, length);
	}

	async flush() {
		return await this.disk.flush();
	}
}

exports.DiskWrapper = DiskWrapper;
