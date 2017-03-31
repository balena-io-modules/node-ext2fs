const bindings = require('bindings')('bindings');

bindings.init();

exports.constants = require('./constants');

exports.trim = function(fs, callback) {
	bindings.trim(fs, callback);
}

exports.mount = function(disk, callback) {
	bindings.mount(disk.request.bind(disk), callback);
}
