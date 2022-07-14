'use strict';

const { ERRNO_TO_CODE } = require('./wasi');
const Module = require('./libext2fs');
const queue = require('./queue');

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
	const {stack} = new Error;
	const result = await queue.addOperation(Module.ccall, [name, returnType, argsType, args, { async: true }]);
	if (result < 0) {
		const err = new ErrnoException(-result, name, args);
		err.stack += stack.substring(stack.indexOf('\n'));
		throw err;
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

function rstripSlashesBuffer(buf) {
	while (buf.length > 1 && buf[buf.length - 1] === 0x2f) {
		buf = buf.slice(0, buf.length - 1);
	}
	return buf;
}

async function withPathAsHeapBuffer(path, fn) {
	// path is a string or a Buffer
	// Strips trailing slashes and converts path to a NULL terminated char* readable from C
	if (!Buffer.isBuffer(path)) {
		path = Buffer.from(path);
	}
	path = rstripSlashesBuffer(path);
	const length = path.length + 1;
	return await withHeapBuffer(length, async (heapBuffer, heapBufferPointer) => {
		heapBuffer[length - 1] = 0;
		path.copy(heapBuffer);
		return await fn(heapBufferPointer);
	});
}
exports.withPathAsHeapBuffer = withPathAsHeapBuffer;

const getIdManager = () => {
	let id = 0;
	const ids = [];
	const getId = () => {
		if (!ids.length) {
			return id++;
		}
		return ids.shift();
	};

	const releaseId = id =>{
		ids.push(id);
	};
	return { releaseId, getId };
};

const { getId:getHookId, releaseId:releaseHookId } = getIdManager();

// inspired by react hooks
let hookId;
const cleanupFns = new Map();

class HookError extends Error {
	constructor() {
		super();
		this.message = 'Cannot use hooks outside hookified function. Did you await hook?';
	}
}

const curHookId = () => {
	if (typeof hookId === 'undefined') {
		throw new HookError;
	}
	return hookId;
};

const getCleanupFns = hookId => {
	const cFns = cleanupFns.get(hookId);
	if (typeof cFns === 'undefined') {
		throw new HookError;
	}
	return cFns;
};
function withHooks(fn) {
	return async (...args) => {
		const oldHookId = hookId;
		const _hookId = hookId = getHookId();
		cleanupFns.set(hookId, []);
		// TODO(zwhitchcox): figure out why the stack trace is lost
		const {stack} = (new Error);
		try {
			return await fn(...args);
		} catch (err) {
			err.stack += stack.substring(stack.indexOf('\n'));
			throw err;
		} finally {
			await Promise.all(cleanupFns.get(_hookId));
			cleanupFns.delete(_hookId);
			hookId = oldHookId;
			releaseHookId(_hookId);
		}
	};
}
exports.withHooks = withHooks;


const checkPath = path => {
	if (('' + path).indexOf('\u0000') !== -1) {
		const er = new Error('Path must be a string without null bytes');
		er.code = 'ENOENT';
		throw er;
	}
};
const usePath = async path => {
	checkPath(path);
	path = rstripSlashesBuffer(Buffer.from(path));
	const [buffer, pointer] = await useBuffer(path.length + 1);
	path.copy(buffer);
	buffer[path.length] = 0;
	return pointer;
};
exports.usePath = usePath;

const usePaths = (...paths) => Promise.all(paths.map(usePath));
exports.usePaths = usePaths;

const useBuffer = async (length) => {
	const _hookId = curHookId();
	const pointer = await ccallThrow('malloc_from_js', 'number', ['number'], [length]);
	const buffer = Module.getBuffer(pointer, length);
	getCleanupFns(_hookId).push(() =>
		ccallThrow('free_from_js', 'void', ['number'], [pointer])
	);
	hookId = _hookId;
	return [buffer, pointer];
};
exports.useBuffer = useBuffer;

const useObject = async obj => {
	const id = Module.setObject(obj);
	const cleanup = getCleanupFns(curHookId());
	cleanup.push(() => Module.deleteObject(id));
	return [obj, id];
};

exports.useObject = useObject;