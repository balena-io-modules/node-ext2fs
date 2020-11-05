/*global Module*/

// Make js objects accessible from C

const objects = new Map();

let nextId = 0;
const idPool = [];

function reserveId() {
	if (idPool.length === 0) {
		nextId += 1;
		idPool.push(nextId);
	}
	return idPool.shift();
}

function releaseId(id) {
	idPool.push(id);
}

function setObject(obj) {
	const id = reserveId();
	objects.set(id, obj);
	return id;
}
Module.setObject = setObject;

function getObject(id) {
	return objects.get(id);
}
Module.getObject = getObject;

function deleteObject(id) {
	objects.delete(id);
	releaseId(id);
}
Module.deleteObject = deleteObject;

async function withObjectId(obj, fn) {
	const id = setObject(obj);
	try {
		return await fn(id);
	} finally {
		deleteObject(id);
	}
}
Module.withObjectId = withObjectId;

// Returns a js Buffer of the memory at `pointer`.
function getBuffer(pointer, length) {
	return Buffer.from(Module.HEAP8.buffer, pointer, length);
}
Module.getBuffer = getBuffer;

// from lib/wasi.js
Module.EIO = 29;
