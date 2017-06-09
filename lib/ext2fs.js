const bindings = require('bindings')('bindings');

bindings.init();

exports.constants = require('./constants');
const diskModule = require('./disk');

exports.trim = function(fs, callback) {
	bindings.trim(fs.filesystem, callback);
};

exports.mount = function(disk, options, callback) {
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
			callback(err);
			return;
		}
		const fs = require('./fs')(filesystem, options);
		fs.filesystem = filesystem;
		callback(null, fs);
	});
};

exports.umount = function(fs, callback) {
	// TODO: ensure all file descriptors are closed
	fs.closeAllFileDescriptors(function(err) {
		if (err) {
			callback(err);
			return;
		}
		bindings.umount(fs.filesystem, callback);
	});
};

exports.close = bindings.closeExt;
