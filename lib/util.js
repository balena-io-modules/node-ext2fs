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

async function ccallThrowAsync(name, returnType, argsType, args) {
	const result = await queue.addOperation(Module.ccall, [name, returnType, argsType, args, { async: true }]);
	if (result < 0) {
		throw new ErrnoException(-result, name, args);
	}
	return result;
}
exports.ccallThrowAsync = ccallThrowAsync;

function ccallThrow(name, returnType, argsType, args) {
	const result = Module.ccall(name, returnType, argsType, args);
	if (result < 0) {
		throw new ErrnoException(-result, name, args);
	}
	return result;
}
exports.ccallThrow = ccallThrow;

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
const objIds = new Map();
const memAddrs = new Map();

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

const free = addr => ccallThrowAsync('free_from_js', 'void', ['number'], [addr]);
const malloc = length => ccallThrowAsync('malloc_from_js', 'void', ['number'], [length]);

function withHooks(fn) {
	return async (...args) => {
		const oldHookId = hookId;
		const _hookId = hookId = getHookId();
		objIds.set(hookId, []);
		memAddrs.set(hookId, []);
		try {
			return await fn(...args);
		} finally {
			for (const objId of objIds.get(_hookId))
				Module.deleteObject(objId);
			await Promise.all(memAddrs.get(_hookId).map(free));
			objIds.delete(_hookId);
			memAddrs.delete(_hookId);
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
const rstripSlashesBuffer = buf => {
	let end = buf.length;
	while (buf[--end] === 0x2f)
		;
	return buf.slice(0, end+1);
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

const useBuffer = async length => {
	const _hookId = curHookId();
	const pointer = await malloc(length);
	const buffer = Module.getBuffer(pointer, length);
	memAddrs.get(_hookId).push(pointer);
	hookId = _hookId;
	return [buffer, pointer];
};
exports.useBuffer = useBuffer;

const useObject = async obj => {
	const id = Module.setObject(obj);
	objIds.get(curHookId()).push(id);
	return [obj, id];
};

exports.useObject = useObject;