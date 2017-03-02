const bindings = require('bindings')('bindings');

bindings.init();

exports.disk = require('./disk');

exports.trim = function(fs, callback) {
	bindings.trim(fs, callback);
}

exports.mount = function(disk, callback) {
	bindings.mount(disk.request.bind(disk), callback);
}
