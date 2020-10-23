'use strict';

const { ERRNO_TO_CODE } = require('./wasi');
const Module = require('./libext2fs');
const queue = require('./queue');

async function promiseToCallback(promise, callback) {
	try {
		const result = await promise;
		callback(null, result);
	} catch (error) {
		callback(error);
	}
}
exports.promiseToCallback = promiseToCallback;

class ErrnoException extends Error {
	constructor(errno, syscall, args) {
		const code = ERRNO_TO_CODE[errno] || 'UNKNOWN';
		super(`${syscall} ${code} (${errno}) args: ${JSON.stringify(args)}`);
		this.name = 'ErrnoException';
		this.errno = errno;
		this.syscall = syscall;
		this.code = code;
	}
}
exports.ErrnoException = ErrnoException;

function ccallThrow(name, returnType, argsType, args) {
	const result = Module.ccall(name, returnType, argsType, args);
	if (result < 0) {
		throw new ErrnoException(-result, name, args);
	}
	return result;
}
exports.ccallThrow = ccallThrow;

async function ccallThrowAsync(name, returnType, argsType, args) {
	const result = await queue.addOperation(Module.ccall, [name, returnType, argsType, args, { async: true }]);
	if (result < 0) {
		throw new ErrnoException(-result, name, args);
	}
	return result;
}
exports.ccallThrowAsync = ccallThrowAsync;

async function withHeapBuffer(length, fn) {
	const heapBufferPointer = ccallThrow('malloc_from_js', 'number', ['number'], [length]);
	const heapBuffer = Module.getBuffer(heapBufferPointer, length);
	try {
		return await fn(heapBuffer, heapBufferPointer);
	} finally {
		ccallThrow('free_from_js', 'void', ['number'], [heapBufferPointer]);
	}
}
exports.withHeapBuffer = withHeapBuffer;

async function withStringOrBufferAsHeapBuffer(stringOrBuffer, fn) {
	// Converts stringOrBuffer to a NULL terminated char* readable from C
	const length = stringOrBuffer.length + 1;
	return await withHeapBuffer(length, async (heapBuffer, heapBufferPointer) => {
		heapBuffer[length - 1] = 0;
		if (Buffer.isBuffer(stringOrBuffer)) {
			stringOrBuffer.copy(heapBuffer);
		} else {
			heapBuffer.write(stringOrBuffer);
		}
		return await fn(heapBufferPointer);
	});
}
exports.withStringOrBufferAsHeapBuffer = withStringOrBufferAsHeapBuffer;
