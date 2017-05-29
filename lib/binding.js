'use strict';

const constants = require('./constants');
const bindings = require('bindings')('bindings');

const openFiles = {}
let currentFd = 1;

function getCallback(req) {
	if (!req || (typeof req.oncomplete !== 'function')) {
		throw new Error('A callback is required.');
	}
	return req.oncomplete.bind(req);
}

function addNullTerminationIfNeeded(s) {
	// TODO: do we need this function anywhere ?
	// If the s is a Buffer, we need to add a NULL termination byte.
	if (Buffer.isBuffer(s)) {
		const nullTerminated = Buffer.allocUnsafe(s.length + 1);
		s.copy(nullTerminated);
		nullTerminated.writeUInt8(0, s.length);
		s = nullTerminated;
	}
	return s;
}

module.exports = function(filesystem) {

	const exports = {};

	exports.FSReqWrap = function () {};

	let Stats;

	exports.FSInitialize = function (s) {
		Stats = s;
	};

	exports.access = function(path, mode, req) {
		throw new Error('Unimplemented');
	};

	exports.chmod = function(path, mode, req) {
		throw new Error('Unimplemented');
	};

	exports.chown = function() {
		throw new Error('Unimplemented');
	};

	exports.close = function(fd, req) {
		const file = openFiles[fd];
		bindings.close(file.file, file.flags, function(err) {
			const callback = getCallback(req);
			if (err)	{
				callback(err);
				return;
			}
			delete openFiles[fd];
			callback(null);
		});
	};

	exports.fchmod = function() {
		throw new Error('Unimplemented');
	};

	exports.fchown = function() {
		throw new Error('Unimplemented');
	};

	exports.fdatasync = function() {
		throw new Error('Unimplemented');
	};

	exports.fstat = function(fd, req) {
		// TODO: handle unexistant fd error case
		bindings.fstat(
			openFiles[fd].file,
			openFiles[fd].flags,
			Stats, getCallback(req)
		);
	};

	exports.fsync = function() {
		throw new Error('Unimplemented');
	};

	exports.ftruncate = function() {
		throw new Error('Unimplemented');
	};

	exports.futimes = function() {
		throw new Error('Unimplemented');
	};

	exports.link = function() {
		throw new Error('Unimplemented');
	};

	exports.lstat = function() {
		throw new Error('Unimplemented');
	};

	exports.mkdir = function(path, mode, req) {
		throw new Error('Unimplemented');
	};

	exports.mkdtemp = function() {
		throw new Error('Unimplemented');
	};

	exports.open = function(path, flags, mode, req) {
		bindings.open(
			filesystem,
			path,
			flags,
		  	mode,
		  	function(err, file) {
		  		const callback = getCallback(req);
		  		if (err) {
		  			callback(err);
		  			return
		  		}
				const fd = currentFd;
				currentFd += 1;
				openFiles[fd] = { file: file, flags: flags };
		  		callback(null, fd);
		  	}
	  	);
	};

	exports.read = function(fd, buffer, offset, length, position, req) {
		// TODO: handle unexistant fd error case
		bindings.read(
			openFiles[fd].file,
			openFiles[fd].flags,
			buffer,
			offset,
			length,
			(position === null) ? -1 : position,
			getCallback(req)
		);
	};

	exports.readdir = function(path, encoding, req) {
		bindings.readdir(filesystem, path, function(err, filenames) {
			const callback = getCallback(req);
			if (err) {
				callback(err);
				return;
			}
			if (encoding === undefined) {
				encoding = 'utf8';
			}
			if (encoding !==  'buffer') {
				filenames = filenames.map(function(buf) {
					return buf.toString(encoding);
				})
			}
			callback(null, filenames);
		});
	};

	exports.readlink = function() {
		throw new Error('Unimplemented');
	};

	exports.rename = function() {
		throw new Error('Unimplemented');
	};

	exports.rmdir = function(path, req) {
		throw new Error('Unimplemented');
	};

	exports.stat = function() {
		throw new Error('Unimplemented');
	};

	exports.StatWatcher = function() {
		throw new Error('Unimplemented');
	};

	exports.symlink = function() {
		throw new Error('Unimplemented');
	};

	exports.unlink = function(path, req) {
		throw new Error('Unimplemented');
	};

	exports.utimes = function() {
		throw new Error('Unimplemented');
	};

	exports.writeBuffer = function(fd, buffer, offset, length, position, req) {
		bindings.write(
			openFiles[fd].file,
			openFiles[fd].flags,
			buffer,
			offset,
			length,
			position,
			getCallback(req)
		);
	};

	exports.writeBuffers = function() {
		throw new Error('Unimplemented');
	};

	exports.writeString = function(fd, string, position, enc, req) {
		const buffer = Buffer.from(string, enc);
		exports.writeBuffer(fd, buffer, 0, buffer.length, position, req);
	};

	return exports;
};
