const lib = require('./libext2fs')
const diskModule = require('./disk');

exports.constants = require('./constants');

exports.trim = function(fs, callback) {
	let cbPointer;
	function cb(err) {
		lib.Runtime.removeFunction(cbPointer);
		callback(err);
	}
	cbPointer = lib.Runtime.addFunction(cb);
	lib.__Z4trimjPFvlE(fs, cbPointer);
}

exports.mount = function(disk, callback) {
	let cbPointer;
	const wrapper = new diskModule.DiskWrapper(disk);
	// TODO: remove when done using it (in umount)
	const requestPointer = lib.Runtime.addFunction(wrapper.request.bind(wrapper));
	function cb(err, fsPointer) {
		lib.Runtime.removeFunction(cbPointer);
		callback(err, fsPointer);
	}
	cbPointer = lib.Runtime.addFunction(cb);
	lib.__Z5mountPFvtmmPcPiEPFvlP18struct_ext2_filsysE(requestPointer, cbPointer);
}
