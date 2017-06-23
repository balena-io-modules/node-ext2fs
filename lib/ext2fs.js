const Promise = require('bluebird');
const bindings = require('bindings')('bindings');

exports.constants = require('./constants');
const diskModule = require('./disk');
let mounts = 0;

exports.mount = function(disk, options, callback) {
	if (mounts === 0) {
		bindings.init();
	}
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
		mounts += 1;
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
			if (mounts === 0) {
				bindings.closeExt();
			}
			callback(err);
		});
	});
};

exports.mountDisposer = function(disk, options) {
	const mountAsync = Promise.promisify(exports.mount);
	const umountAsync = Promise.promisify(exports.umount);
	return mountAsync(disk, options || {})
	.disposer(function(fs) {
		return umountAsync(fs);
	});
};
