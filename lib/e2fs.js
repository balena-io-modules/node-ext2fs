const bindings = require('bindings')('bindings');

const fs = require('fs');

const constants = {
	E2FS_BLK_OPEN:             0,
	E2FS_BLK_CLOSE:            1,
	E2FS_BLK_READ:             2,
	E2FS_BLK_WRITE:            3,
	E2FS_BLK_FLUSH:            4,
	E2FS_BLK_DISCARD:          10,
	E2FS_BLK_CACHE_READAHEAD:  11,
	E2FS_BLK_ZEROOUT:          12,
}

let fd = -1;

function request(type, offset, buffer, callback) {
	console.log('Got request type', type);
	switch (type) {
		case constants.E2FS_BLK_OPEN:
			fs.open('/home/petrosagg/projects/node-e2fsprogs/foo.ext4', 'r', function (err, _fd) {
				fd = _fd;
				callback(null);
			});
			break;
		case constants.E2FS_BLK_READ:
			console.log('\tJS: readBlock: offset', offset, 'length', buffer.length);
			fs.read(fd, buffer, 0, buffer.length, offset, callback);
			break;
		default:
			callback(null);
	}
}

bindings.mount(request, function() {
	console.log('done mounting filesystem');
})
