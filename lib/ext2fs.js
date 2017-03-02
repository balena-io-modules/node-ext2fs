const bindings = require('bindings')('bindings');

bindings.init();

exports.disk = require('./disk');

exports.mount = function(disk, callback) {
	bindings.mount(disk.request.bind(disk), callback);
}
