'use strict';

const bindings = require('bindings')('bindings');
const constants = require('./constants');
const fs = require('fs');  // original fs module, used for its constants
const util = require('util');

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

module.exports = function(filesystem, options) {
	options = options || {};
	const MAX_FD = options.MAX_FD || 1024;
	const openFiles = new Array(MAX_FD);
	function getFd() {
		for (let i=0; i<MAX_FD; i++) {
			if (openFiles[i] === undefined) {
				return i;
			}
		}
		return -1;
	}

	const exports = {};

	exports.FSReqWrap = function () {};

	let Stats;

	exports.FSInitialize = function (s) {
		Stats = s;
	};

	exports.access = function(path, mode, req) {
		const callback = getCallback(req);
		const X = (
			fs.constants.S_IXUSR |
			fs.constants.S_IXGRP |
			fs.constants.S_IXOTH
		);
		stat(path, function(err, stats) {
			if (err) {
				callback(err);
				return;
			}
			if (((mode & fs.constants.X_OK) !== 0) && ((stats.mode & X) === 0)) {
				callback({
					Error: `permission denied, access '${path}'`,
					errno: 13,
					code: 'EACCES'
				});
				return;
			}
			callback(null);
		})
	};

	exports.chmod = function(path, mode, req) {
		const callback = getCallback(req);
		open(path, 'r', 0, function(err, fd) {
			if (err) {
				callback(err);
				return;
			}
			fchmod(fd, mode, function(err) {
				if (err) {
					callback(err);
					return;
				}
				close(fd, callback);
			});
		})
	};

	exports.chown = function(path, uid, gid, req) {
		const callback = getCallback(req);
		open(path, 'r', 0, function(err, fd) {
			if (err) {
				callback(err);
				return;
			}
			fchown(fd, uid, gid, function(err) {
				if (err) {
					callback(err);
					return;
				}
				close(fd, callback);
			});
		})
	};

	const close = function(fd, callback) {
		const file = openFiles[fd];
		bindings.close(file.file, file.flags, function(err) {
			if (err)	{
				callback(err);
				return;
			}
			delete openFiles[fd];
			callback(null);
		});
	};

	exports.close = function(fd, req) {
		close(fd, getCallback(req));
	};

	const fchmod = function(fd, mode, callback) {
		const file = openFiles[fd];
		bindings.fchmod(file.file, file.flags, mode, callback);
	};

	exports.fchmod = function(fd, mode, req) {
		fchmod(fd, mode, getCallback(req));
	};

	const fchown = function(fd, uid, gid, callback) {
		const file = openFiles[fd];
		bindings.fchown(file.file, file.flags, uid, gid, callback)
	};

	exports.fchown = function(fd, uid, gid, req) {
		fchown(fd, uid, gid, getCallback(req));
	};

	exports.fdatasync = function() {
		throw new Error('Unimplemented');
	};

	const fstat = function(fd, callback) {
		// TODO: handle unexistant fd error case
		bindings.fstat(
			openFiles[fd].file,
			openFiles[fd].flags,
			Stats,
			callback
		);
	};

	exports.fstat = function(fd, req) {
		fstat(fd, getCallback(req));
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
		bindings.mkdir(filesystem, path, mode, getCallback(req));
	};

	exports.mkdtemp = function() {
		throw new Error('Unimplemented');
	};

	const open = function(path, flags, mode, callback) {
		bindings.open(
			filesystem,
			path,
			flags,
		  	mode,
		  	function(err, file) {
		  		if (err) {
		  			callback(err);
		  			return
		  		}
				const fd = getFd();
				if (fd === -1) {
					callback({
						Error: 'Too many open files',
						errno: 24,
						code: 'EMFILE'
					});
					return;
				}
				openFiles[fd] = { file: file, flags: flags };
		  		callback(null, fd);
		  	}
	  	);
	};

	exports.open = function(path, flags, mode, req) {
		open(path, flags, mode, getCallback(req));
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
		bindings.rmdir(filesystem, path, getCallback(req));
	};

	const stat = function(path, callback) {
		// TODO: noatime ?
		open(path, 'r', 0, function(err, fd) {
			if (err) {
				callback(err);
				return;
			}
			fstat(fd, function(err, stats) {
				if (err) {
					callback(err);
					return;
				}
				close(fd, function(err) {
					if (err) {
						callback(err);
						return;
					}
					callback(null, stats);
				});
			});
		});
	};

	exports.stat = function(path, req) {
		stat(path, getCallback(req));
	};

	exports.StatWatcher = function() {
		throw new Error('Unimplemented');
	};

	exports.symlink = function() {
		throw new Error('Unimplemented');
	};

	exports.unlink = function(path, req) {
		bindings.unlink(filesystem, path, getCallback(req));
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
