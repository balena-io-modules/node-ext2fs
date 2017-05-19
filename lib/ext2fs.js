const bindings = require('bindings')('bindings');

bindings.init();

exports.constants = require('./constants');
const diskModule = require('./disk');

exports.trim = function(fs, callback) {
	bindings.trim(fs.filesystem, callback);
};

exports.mount = function(disk, callback) {
	const wrapper = new diskModule.DiskWrapper(disk);
	bindings.mount(wrapper.request.bind(wrapper), function(err, filesystem) {
		if (err) {
			callback(err);
			return;
		}
		const fs = require('./fs')(filesystem);
		fs.filesystem = filesystem;
		callback(null, fs);
	});
};

exports.umount = function(fs, callback) {
	bindings.umount(fs.filesystem, callback);
};

exports.close = bindings.closeExt;
