'use strict';

const { DiskWrapper } = require('./disk');
const createFs = require('./fs');
const Module = require('./libext2fs');
const { ccallThrowAsync } = require('./util');

const ready = new Promise((resolve) => {
	Module.onRuntimeInitialized = resolve;
});

exports.mount = async function(disk, offset = 0) {
	await ready;
	const wrapper = new DiskWrapper(disk, offset);
	const diskId = Module.setObject(wrapper);
	let fsPointer;
	try {
		fsPointer = await ccallThrowAsync('node_ext2fs_mount', 'number', ['number'], [diskId]);
	} catch (error) {
		Module.deleteObject(diskId);
		throw error;
	}
	const [fs, fsPromises] = createFs(fsPointer);
	fs.trim = fsPromises.trim = async () => {
		await ccallThrowAsync('node_ext2fs_trim', 'number', ['number'], [fsPointer]);
	};
	fs.diskId = fsPromises.trim = diskId;
	return [fs, fsPromises];
};

exports.umount = async function(fs) {
	await fs.closeAllFileDescriptors();
	await ccallThrowAsync('node_ext2fs_umount', 'number', ['number'], [fs.fsPointer]);
	Module.deleteObject(fs.diskId);
};

exports.withMountedDisk = async function(disk, offset, fn) {
	const [fs, fsPromises] = await exports.mount(disk, offset);
	try {
		return await fn(fs, fsPromises);
	} finally {
		await exports.umount(fs);
	}
};
