const { promisify } = require('util');

const bindings = require('bindings')('bindings');

exports.constants = require('./constants');
const diskModule = require('./disk');
let mounts = 0;

function closeIfNoMountsLeft() {
	if (mounts === 0) {
		bindings.closeExt();
	}
}

exports.mount = function(disk, options, callback) {
	if (mounts === 0) {
		bindings.init();
	}
	mounts += 1;
	if (callback === undefined) {
		callback = options;
		options = {};
	}
	const offset = options.offset || 0;
	delete options.offset;
	const wrapper = new diskModule.DiskWrapper(disk, offset);
	const request = wrapper.request.bind(wrapper);
	bindings.mount(request, function(err, filesystem) {
		if (err) {
			mounts -= 1;
			closeIfNoMountsLeft();
			callback(err);
			return;
		}
		const fs = require('./fs')(filesystem, options);
		fs.filesystem = filesystem;
		fs.trim = function(callback) {
			bindings.trim(filesystem, callback);
		};
		callback(null, fs);
	});
};

exports.umount = function(fs, callback) {
	fs.closeAllFileDescriptors(function(err) {
		if (err) {
			callback(err);
			return;
		}
		bindings.umount(fs.filesystem, function(err) {
			mounts -= 1;
			closeIfNoMountsLeft();
			callback(err);
		});
	});
};

const mountAsync = promisify(exports.mount);
const umountAsync = promisify(exports.umount);

exports.withMountedDisk = async function(disk, options, fn) {
	const fs = await mountAsync(disk, options || {});
	try {
		return await fn(fs);
	} finally {
		await umountAsync(fs);
	}
};
