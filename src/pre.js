// Make js objects accessible from C

const objects = new Map();

let objectId = 0;

function setObject(obj) {
	objectId += 1;
	objects.set(objectId, obj);
	return objectId;
}
Module.setObject = setObject;

function getObject(id) {
	return objects.get(id);
}
Module.getObject = getObject;

function deleteObject(id) {
	objects.delete(id);
}
Module.deleteObject = deleteObject;

// Returns a js Buffer of the memory at `pointer`.
function getBuffer(pointer, length) {
	return Buffer.from(Module.HEAP8.buffer, pointer, length);
}
Module.getBuffer = getBuffer;

// from lib/wasi.js
Module.EIO = 29;
