'use strict';

const bindings = require('bindings')('bindings');

const constants = require('./constants');
const queue = require('./queue');
const fs = require('fs');  // original fs module, used for its constants
const util = require('util');

function getCallback(req) {
	if (!req || (typeof req.oncomplete !== 'function')) {
		throw new Error('A callback is required.');
	}
	return req.oncomplete.bind(req);
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

	function getFile(fd, callback) {
		const file = openFiles[fd];
		if (file === undefined) {
			const error = new Error('bad file descriptor');
			error.errno = 9;
			error.code = 'EBADF';
			callback(error);
			return null;
		}
		return file;
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
				const error = new Error(`permission denied, access '${path}'`);
				error.errno = 13;
				error.code = 'EACCES';
				callback(error);
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
		const file = getFile(fd, callback);
		if (file === null) {
			return;
		}
		queue.addOperation(bindings.close, [
			file.file,
			file.flags,
			function(err) {
				if (err)	{
					callback(err);
					return;
				}
				delete openFiles[fd];
				callback(null);
			}
		]);
	};

	exports.close = function(fd, req) {
		close(fd, getCallback(req));
	};

	const fchmod = function(fd, mode, callback) {
		const file = getFile(fd, callback);
		if (file === null) {
			return;
		}
		queue.addOperation(bindings.fchmod, [
			file.file,
			file.flags,
			mode,
			callback
		]);
	};

	exports.fchmod = function(fd, mode, req) {
		fchmod(fd, mode, getCallback(req));
	};

	const fchown = function(fd, uid, gid, callback) {
		const file = getFile(fd, callback);
		if (file === null) {
			return;
		}
		queue.addOperation(bindings.fchown, [
			file.file,
			file.flags,
			uid,
			gid,
			callback
		]);
	};

	exports.fchown = function(fd, uid, gid, req) {
		fchown(fd, uid, gid, getCallback(req));
	};

	exports.fdatasync = function() {
		throw new Error('Unimplemented');
	};

	const fstat = function(fd, callback) {
		const file = getFile(fd, callback);
		if (file === null) {
			return;
		}
		queue.addOperation(bindings.fstat_, [
			file.file,
			file.flags,
			Stats,
			callback
		]);
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
		queue.addOperation(bindings.mkdir, [
			filesystem,
			path,
			mode,
			getCallback(req)
		]);
	};

	exports.mkdtemp = function() {
		throw new Error('Unimplemented');
	};

	const open = function(path, flags, mode, callback) {
		if (!path.startsWith('/')) {
			path = '/' + path;
		}
		queue.addOperation(bindings.open, [
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
					const error = new Error('Too many open files');
					error.errno = 24;
					error.code = 'EMFILE';
					callback(error);
					return;
				}
				openFiles[fd] = { file: file, flags: flags };
		  		callback(null, fd);
		  	}
		]);
	};

	exports.open = function(path, flags, mode, req) {
		open(path, flags, mode, getCallback(req));
	};

	exports.read = function(fd, buffer, offset, length, position, req) {
		const callback = getCallback(req);
		const file = getFile(fd, callback);
		if (file === null) {
			return;
		}
		queue.addOperation(bindings.read, [
			file.file,
			file.flags,
			buffer,
			offset,
			length,
			(typeof position !== 'number') ? -1 : position,
			callback
		]);
	};

	exports.readdir = function(path, encoding, req) {
		queue.addOperation(bindings.readdir, [
			filesystem,
			path,
			function(err, filenames) {
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
			}
		]);
	};

	exports.readlink = function() {
		throw new Error('Unimplemented');
	};

	exports.rename = function() {
		throw new Error('Unimplemented');
	};

	exports.rmdir = function(path, req) {
		queue.addOperation(bindings.rmdir, [
			filesystem,
			path,
			getCallback(req)
		]);
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
		queue.addOperation(bindings.unlink_, [
			filesystem,
			path,
			getCallback(req)
		]);
	};

	exports.utimes = function() {
		throw new Error('Unimplemented');
	};

	exports.writeBuffer = function(fd, buffer, offset, length, position, req) {
		const callback = getCallback(req);
		const file = getFile(fd, callback);
		if (file === null) {
			return;
		}
		queue.addOperation(bindings.write, [
			file.file,
			file.flags,
			buffer,
			offset,
			length,
			position,
			callback
		]);
	};

	exports.writeBuffers = function() {
		throw new Error('Unimplemented');
	};

	exports.writeString = function(fd, string, position, enc, req) {
		const buffer = Buffer.from(string, enc);
		exports.writeBuffer(fd, buffer, 0, buffer.length, position, req);
	};

	exports.closeAllFileDescriptors = function(callback) {
		const fds = [];
		openFiles.forEach(function(file, fd) {
			if (file !== undefined) {
				fds.push(fd);
			}
		});
		const closeNext = function() {
			if (fds.length) {
				const fd = fds.pop();
				close(fd, function(err) {
					if (err) {
						callback(err);
						return;
					}
					closeNext();
				});
			} else {
				callback(null);
			}
		}
		closeNext();
	}

	return exports;
};
