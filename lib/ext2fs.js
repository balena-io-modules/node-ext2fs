const lib = require('./libext2fs')
const diskModule = require('./disk');

exports.constants = require('./constants');

const diskRequestPointers = new Map();

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
	const requestPointer = lib.Runtime.addFunction(wrapper.request.bind(wrapper));
	function cb(err, fsPointer) {
		diskRequestPointers[fsPointer] = requestPointer;
		lib.Runtime.removeFunction(cbPointer);
		callback(err, fsPointer);
	}
	cbPointer = lib.Runtime.addFunction(cb);
	lib.__Z5mountPFvtmmPcPiEPFvlP18struct_ext2_filsysE(requestPointer, cbPointer);
}

exports.umount = function(fsPointer, callback) {
	let cbPointer;
	const diskRequestPointer = diskRequestPointers[fsPointer];
	function cb(err) {
		lib.Runtime.removeFunction(diskRequestPointer);
		diskRequestPointers.delete(fsPointer)
		lib.Runtime.removeFunction(cbPointer);
		callback(err);
	}
	cbPointer = lib.Runtime.addFunction(cb);
	lib.__Z6umountjPFvlE(fsPointer, cbPointer);
}
