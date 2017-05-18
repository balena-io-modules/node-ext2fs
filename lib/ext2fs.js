const bindings = require('bindings')('bindings');

bindings.init();

exports.constants = require('./constants');
const diskModule = require('./disk');

exports.trim = function(fs, callback) {
	bindings.trim(fs, callback);
}

exports.mount = function(disk, callback) {
	const wrapper = new diskModule.DiskWrapper(disk);
	bindings.mount(wrapper.request.bind(wrapper), callback);
}

exports.umount = function(filesystem, callback) {
	bindings.umount(filesystem, callback);
}

exports.close = bindings.close;
