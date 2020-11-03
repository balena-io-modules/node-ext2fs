'use strict';

const Module = require('./libext2fs');
const { CODE_TO_ERRNO } = require('./wasi');
const {
	ErrnoException,
	ccallThrow,
	ccallThrowAsync,
	promiseToCallback,
	withHeapBuffer,
	withPathAsHeapBuffer,
} = require('./util');

function getCallback(req) {
	if (!req || (typeof req.oncomplete !== 'function')) {
		throw new Error('A callback is required.');
	}
	return req.oncomplete.bind(req);
}

module.exports = function(constants, fsPointer) {
	const exports = {};

	const openFiles = new Map();
	exports.openFiles = openFiles;

	exports.FSReqWrap = function () {};

	let Stats;

	exports.FSInitialize = function (s) {
		Stats = s;
	};

	async function access(path, mode) {
		const X = (
			constants.S_IXUSR |
			constants.S_IXGRP |
			constants.S_IXOTH
		);
		const stats = await stat(path);
		if (((mode & constants.X_OK) !== 0) && ((stats.mode & X) === 0)) {
			throw new ErrnoException(CODE_TO_ERRNO['EACCES'], 'access', [path, mode]);
		}
	}

	exports.access = function(path, mode, req) {
		promiseToCallback(
			access(path, mode),
			getCallback(req),
		);
	};

	function checkFd(fd, syscall, args) {
		if (!openFiles.has(fd)) {
			throw new ErrnoException(CODE_TO_ERRNO['EBADF'], syscall, args);
		}
	}

	async function chmod(path, mode) {
		const fd = await open(path, 0, 0);
		await fchmod(fd, mode);
	}

	exports.chmod = function(path, mode, req) {
		promiseToCallback(
			chmod(path, mode),
			getCallback(req),
		);
	};

	async function chown(path, uid, gid) {
		const fd = await open(path, 0, 0);
		await fchown(fd, uid, gid);
	}

	exports.chown = function(path, uid, gid, req) {
		promiseToCallback(
			chown(path, uid, gid),
			getCallback(req),
		);
	};

	async function close(fd) {
		checkFd(fd, 'node_ext2fs_close', [fd]);
		await ccallThrowAsync('node_ext2fs_close', 'number', ['number'], [fd]);
		openFiles.delete(fd);
	}

	exports.close = function(fd, req) {
		promiseToCallback(
			close(fd),
			getCallback(req),
		);
	};

	async function fchmod(fd, mode) {
		checkFd(fd, 'node_ext2fs_chmod', [fd, mode]);
		await ccallThrowAsync('node_ext2fs_chmod', 'number', ['number', 'number'], [fd, mode]);
	}

	exports.fchmod = function(fd, mode, req) {
		promiseToCallback(
			fchmod(fd, mode),
			getCallback(req),
		);
	};

	async function fchown(fd, uid, gid) {
		checkFd(fd, 'node_ext2fs_chown', [fd, uid, gid]);
		await ccallThrowAsync('node_ext2fs_chown', 'number', ['number', 'number', 'number'], [fd, uid, gid]);
	}

	exports.fchown = function(fd, uid, gid, req) {
		promiseToCallback(
			fchown(fd, uid, gid),
			getCallback(req),
		);
	};

	exports.fdatasync = function() {
		throw new Error('Unimplemented');
	};

	function fstat(fd) {
		checkFd(fd, 'fstat', [fd]);
		function getAttr(name) {
			return ccallThrow(`node_ext2fs_stat_${name}`, 'number', ['number'], [fd]);
		}
		const ctime = getAttr('i_ctime') * 1000;
		return new Stats(
			0,   // dev
			getAttr('i_mode'),
			getAttr('i_links_count'),
			getAttr('i_uid'),
			getAttr('i_gid'),
			0,  // rdev
			getAttr('blocksize'),
			getAttr('ino'),
			getAttr('i_size'),
			getAttr('i_blocks'),
			getAttr('i_atime') * 1000,
			getAttr('i_mtime') * 1000,
			ctime,
			ctime,
		);
	}

	exports.fstat = function(fd, req) {
		const callback = getCallback(req);
		try {
			callback(null, fstat(fd));
		} catch (error) {
			callback(error);
		}
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

	async function mkdir(path, mode) {
		return await withPathAsHeapBuffer(path, async (heapBufferPointer) => {
			await ccallThrowAsync('node_ext2fs_mkdir', 'number', ['number', 'number', 'number'], [fsPointer, heapBufferPointer, mode]);
		});
	}

	exports.mkdir = function(path, mode, req) {
		promiseToCallback(
			mkdir(path, mode),
			getCallback(req),
		);
	};

	exports.mkdtemp = function() {
		throw new Error('Unimplemented');
	};

	async function open(path, flags, mode) {
		return await withPathAsHeapBuffer(path, async (heapBufferPointer) => {
			const fd = await ccallThrowAsync(
				'node_ext2fs_open',
				'number',
				['number', 'number', 'number', 'number'],
				[fsPointer, heapBufferPointer, flags, mode],
			);
			openFiles.set(fd, flags);
			return fd;
		});
	}

	exports.open = function(path, flags, mode, req) {
		promiseToCallback(
			open(path, flags, mode),
			getCallback(req),
		);
	};

	async function read(fd, buffer, offset, length, position) {
		checkFd(fd, 'node_ext2fs_read', [fd, buffer, offset, length, position]);
		return await withHeapBuffer(length, async (heapBuffer, heapBufferPointer) => {
			const got = await ccallThrowAsync(
				'node_ext2fs_read',
				'number',
				['number', 'number', 'number', 'number', 'number'],
				[fd, openFiles.get(fd), heapBufferPointer, length, position],
			);
			heapBuffer.copy(buffer, offset);
			return got;
		});
	}

	exports.read = function(fd, buffer, offset, length, position, req) {
		promiseToCallback(
			read(
				fd,
				buffer,
				offset,
				length,
				(typeof position !== 'number') ? -1 : position,
			),
			getCallback(req),
		);
	};

	async function readdir(path, encoding) {
		if (encoding === undefined) {
			encoding = 'utf8';
		}
		const array = [];
		const arrayId = Module.setObject(array);
		await withPathAsHeapBuffer(path, async (heapBufferPointer) => {
			await ccallThrowAsync('node_ext2fs_readdir', 'number', ['number', 'number', 'number'], [fsPointer, heapBufferPointer, arrayId]);
		});
		Module.deleteObject(arrayId);
		if (encoding === 'buffer') {
			return array;
		} else {
			return array.map((b) => b.toString(encoding));
		}
	}

	exports.readdir = function(path, encoding, req) {
		promiseToCallback(
			readdir(
				path,
				encoding,
			),
			getCallback(req),
		);
	};

	exports.readlink = function() {
		throw new Error('Unimplemented');
	};

	exports.rename = function() {
		throw new Error('Unimplemented');
	};

	async function unlink(path, isdir) {
		await withPathAsHeapBuffer(path, async (heapBufferPointer) => {
			await ccallThrowAsync(
				'node_ext2fs_unlink',
				'number',
				['number', 'number', 'number'],
				[fsPointer, heapBufferPointer, isdir],
			);
		});
	}

	exports.rmdir = function(path, req) {
		promiseToCallback(
			unlink(path, 1),
			getCallback(req),
		);
	};

	async function stat(path) {
		// TODO: noatime ?
		const fd = await open(path, 'r', 0);
		const stats = await fstat(fd);
		await close(fd);
		return stats;
	}

	exports.stat = function(path, req) {
		promiseToCallback(
			stat(path),
			getCallback(req),
		);
	};

	exports.StatWatcher = function() {
		throw new Error('Unimplemented');
	};

	exports.symlink = function() {
		throw new Error('Unimplemented');
	};

	exports.unlink = function(path, req) {
		promiseToCallback(
			unlink(path, 0),
			getCallback(req),
		);
	};

	exports.utimes = function() {
		throw new Error('Unimplemented');
	};

	async function writeBuffer(fd, buffer, offset, length, position) {
		checkFd(fd, 'node_ext2fs_write', [fd, buffer, offset, length, position]);
		return await withHeapBuffer(length, async (heapBuffer, heapBufferPointer) => {
			buffer.copy(heapBuffer, 0, offset, offset + length);
			return await ccallThrowAsync(
				'node_ext2fs_write',
				'number',
				['number', 'number', 'number', 'number', 'number'],
				[fd, openFiles.get(fd), heapBufferPointer, length, position],
			);
		});
	}

	exports.writeBuffer = function(fd, buffer, offset, length, position, req) {
		promiseToCallback(
			writeBuffer(
				fd,
				buffer,
				offset,
				length,
				(typeof position !== 'number') ? -1 : position,
			),
			getCallback(req),
		);
	};

	exports.writeBuffers = function() {
		throw new Error('Unimplemented');
	};

	exports.writeString = function(fd, string, position, enc, req) {
		const buffer = Buffer.from(string, enc);
		exports.writeBuffer(fd, buffer, 0, buffer.length, position, req);
	};

	exports.closeAllFileDescriptors = async function() {
		for (const fd of openFiles.keys()) {
			await close(fd);
		}
	};

	return exports;
};
