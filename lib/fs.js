// Maintainers, keep in mind that ES1-style octal literals (`0666`) are not
// allowed in strict mode. Use ES6-style octal literals instead (`0o666`).

'use strict';

const Buffer = require('buffer').Buffer;
const Stream = require('stream').Stream;
const EventEmitter = require('events');
const path = require('path');
const { CODE_TO_ERRNO } = require('./wasi');
const {
	ErrnoException,
  usePath,
  useBuffer,
  usePaths,
  withHooks,
  useObject,
} = require('./util');
const { callbackify } = require('util');
const assert = require('assert');
const binding = require('./binding');

const Readable = Stream.Readable;
const Writable = Stream.Writable;

const kMinPoolSpace = 128;
const kMaxLength = require('buffer').kMaxLength;
const kFd = Symbol("fd");

// Same constants for all platforms:
const constants = Object.freeze({
  O_RDONLY: 0,
  O_WRONLY: 1,
  O_RDWR: 2,
  S_IFMT: 61440,
  S_IFREG: 32768,
  S_IFDIR: 16384,
  S_IFCHR: 8192,
  S_IFBLK: 24576,
  S_IFIFO: 4096,
  S_IFLNK: 40960,
  S_IFSOCK: 49152,
  O_CREAT: 64,
  O_EXCL: 128,
  O_NOCTTY: 256,
  O_TRUNC: 512,
  O_APPEND: 1024,
  O_DIRECTORY: 65536,
  O_NOATIME: 262144,
  O_NOFOLLOW: 131072,
  O_SYNC: 1052672,
  O_DIRECT: 16384,
  O_NONBLOCK: 2048,
  S_IRWXU: 448,
  S_IRUSR: 256,
  S_IWUSR: 128,
  S_IXUSR: 64,
  S_IRWXG: 56,
  S_IRGRP: 32,
  S_IWGRP: 16,
  S_IXGRP: 8,
  S_IRWXO: 7,
  S_IROTH: 4,
  S_IWOTH: 2,
  S_IXOTH: 1,
  F_OK: 0,
  R_OK: 4,
  W_OK: 2,
  X_OK: 1
});

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new Error(`Unknown encoding: ${encoding}`);
  }
}

function stringToFlags(flag) {
  if (typeof flag === 'number') {
    return flag;
  }

  switch (flag) {
    case 'r' : return constants.O_RDONLY;
    case 'rs' : // Fall through.
    case 'sr' : return constants.O_RDONLY | constants.O_SYNC;
    case 'r+' : return constants.O_RDWR;
    case 'rs+' : // Fall through.
    case 'sr+' : return constants.O_RDWR | constants.O_SYNC;

    case 'w' : return constants.O_TRUNC | constants.O_CREAT | constants.O_WRONLY;
    case 'wx' : // Fall through.
    case 'xw' : return constants.O_TRUNC | constants.O_CREAT | constants.O_WRONLY | constants.O_EXCL;

    case 'w+' : return constants.O_TRUNC | constants.O_CREAT | constants.O_RDWR;
    case 'wx+': // Fall through.
    case 'xw+': return constants.O_TRUNC | constants.O_CREAT | constants.O_RDWR | constants.O_EXCL;

    case 'a' : return constants.O_APPEND | constants.O_CREAT | constants.O_WRONLY;
    case 'ax' : // Fall through.
    case 'xa' : return constants.O_APPEND | constants.O_CREAT | constants.O_WRONLY | constants.O_EXCL;

    case 'a+' : return constants.O_APPEND | constants.O_CREAT | constants.O_RDWR;
    case 'ax+': // Fall through.
    case 'xa+': return constants.O_APPEND | constants.O_CREAT | constants.O_RDWR | constants.O_EXCL;
  }

  throw new Error('Unknown file open flag: ' + flag);
}

function getOptions(options, defaultOptions) {
  if (options == null) {
    return defaultOptions;
  }

  if (typeof options === 'string') {
    if (defaultOptions === undefined) {
      defaultOptions = {}
    }
    options = {
      ...defaultOptions,
      encoding: options,
    };
  } else if (typeof options !== 'object') {
    throw new TypeError('"options" must be a string or an object, got ' +
                        typeof options + ' instead.');
  }

  if (options.encoding !== 'buffer')
    assertEncoding(options.encoding);
  return {
    ...options,
    ...defaultOptions,
  };
}

function pathCheck(path) {
  if (('' + path).indexOf('\u0000') !== -1) {
    const er = new Error('Path must be a string without null bytes');
    er.code = 'ENOENT';
    throw er;
  }
}

function isFd(path) {
  return (path >>> 0) === path;
}

class Stats {
  constructor(
    dev,
    mode,
    nlink,
    uid,
    gid,
    rdev,
    blksize,
    ino,
    size,
    blocks,
    atim_msec,
    mtim_msec,
    ctim_msec,
    birthtim_msec) {
    this.dev = dev;
    this.mode = mode;
    this.nlink = nlink;
    this.uid = uid;
    this.gid = gid;
    this.rdev = rdev;
    this.blksize = blksize;
    this.ino = ino;
    this.size = size;
    this.blocks = blocks;
    this.atime = new Date(atim_msec);
    this.mtime = new Date(mtim_msec);
    this.ctime = new Date(ctim_msec);
    this.birthtime = new Date(birthtim_msec);
  }
  _checkModeProperty(property) {
    return ((this.mode & constants.S_IFMT) === property);
  };

  isDirectory() {
    return this._checkModeProperty(constants.S_IFDIR);
  }

  isFile() {
    return this._checkModeProperty(constants.S_IFREG);
  };

  isBlockDevice() {
    return this._checkModeProperty(constants.S_IFBLK);
  };

  isCharacterDevice() {
    return this._checkModeProperty(constants.S_IFCHR);
  };

  isSymbolicLink() {
    return this._checkModeProperty(constants.S_IFLNK);
  };

  isFIFO() {
    return this._checkModeProperty(constants.S_IFIFO);
  };

  isSocket() {
    return this._checkModeProperty(constants.S_IFSOCK);
  };
};

function modeNum(m, def) {
  if (typeof m === 'number')
    return m;
  if (typeof m === 'string')
    return parseInt(m, 8);
  if (def)
    return modeNum(def);
  return undefined;
}

// converts Date or number to a fractional UNIX timestamp
function toUnixTimestamp(time) {
  if (typeof time === 'string' && +time == time) {
    return +time;
  }
  if (typeof time === 'number') {
    if (!Number.isFinite(time) || time < 0) {
      return Date.now() / 1000;
    }
    return time;
  }
  if (util.isDate(time)) {
    // convert to 123.456 UNIX timestamp
    return time.getTime() / 1000;
  }
  throw new Error('Cannot parse time: ' + time);
}

class UnimplementedError extends Error {
	constructor(method) {
		super(`\`${method}\` not yet implemented.`);
	}
}


module.exports = fsPointer => {
const {
  X_OK = 0,
  O_RDONLY,
  O_NOFOLLOW,
  S_IXUSR,
  S_IXGRP,
  S_IXOTH,
} = constants;

// TODO(zwhitchcox): Should keep track of position in file here
const openFiles = new Map();
async function closeAllFileDescriptors() {
  for (const fd of openFiles.keys()) {
    await close(fd);
  }
};

function checkFd(fd, syscall, args) {
  if (!openFiles.has(fd)) {
    throw new ErrnoException(CODE_TO_ERRNO['EBADF'], syscall, args);
  }
}

const DEBUG = process.env.NODE_DEBUG && /fs/.test(process.env.NODE_DEBUG);

const EXECUTE_ABILITY = (S_IXUSR | S_IXGRP | S_IXOTH);
const access = async (path, mode) => {
  // we know we can read/write being that we control the file
  // so we just need to check that the file exists and maybe execution ability
  const stats = await stat(path);
  if (((mode & X_OK) !== 0) && ((stats.mode & EXECUTE_ABILITY) === 0)) {
    throw new ErrnoException(CODE_TO_ERRNO['EACCES'], 'access', [path, mode]);
  }
};

const open = withHooks(async (path, flags, mode) => {
  mode = modeNum(mode, 0o666);
  flags = stringToFlags(flags);
  path = await usePath(path);

  const fd = await binding.open(fsPointer, path, flags, mode);
  openFiles.set(fd, flags);
  return fd;
});

async function close(fd) {
  checkFd(fd, 'close', [fd]);
  await binding.close(fd);
  openFiles.delete(fd);
}

const read = withHooks(async (fd, buffer, offset, length, position) => {
  position = (typeof position !== 'number') ? -1 : position;
  if (length === 0) {
    return {
      bytesRead: 0,
      buffer,
    }
  }
  checkFd(fd, 'read', [fd, buffer, offset, length, position]);
  const [_readBuffer, readPointer] = await useBuffer(length);
  const bytesRead = await binding.read(fd, openFiles.get(fd), readPointer, length, position);
  _readBuffer.copy(buffer, offset);
  return {
    bytesRead,
    buffer,
  }
});

const readv = withHooks(async (fd, buffers, position) => {
  if (typeof position !== 'number')
    position = -1;

  let bytesRead = 0;
  for (const buffer of buffers) {
    const { bytesRead:numRead } = await read(fd, buffer, position);
    bytesRead += numRead;
  }
  return {
    bytesRead,
    buffers: Buffer.concat(buffers, bytesRead),
  };
});

function readCb(fd, buffer, offset, length, position, cb) {
  // follow node fs api and don't return promises
  ;(async () => {
    try {
      const { bytesRead, buffer:outBuffer } = await read(fd, buffer, offset, length, position);
      cb(null, bytesRead, outBuffer);
    } catch (err) {
      cb(err);
    }
  })()
}

const write = (fd, stringOrBuffer, offsetOrOptions, length, position) => {
  // fs.write(fd, buffer[, offset[, length[, position]]]);
  if (stringOrBuffer instanceof Buffer) {
    const buffer = stringOrBuffer;
    let offset;
    if (typeof offset === 'object') {
      ({
        offset = 0,
        length = buffer.byteLength - offset,
        position = null,
      } = offsetOrOptions);
    }
    if (typeof offsetOrOptions !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.byteLength - offset;
    return writeBuffer(fd, buffer, offset, length, position);
  }
  if (typeof stringOrBuffer !== "string") {
    throw new Error(`Argument must be buffer or string. Got ${typeof buffer}`);
  }

  // fs.write(fd, string[, position[, encoding]]);
  const buffer = Buffer.from(String(stringOrBuffer));
  const encoding = length !== undefined ? length : 'utf8';
  position = offsetOrOptions;
  return writeBuffer(fd, buffer, 0, buffer.byteLength, position)
    .then(({bytesWritten, buffer}) => {
      return {
        bytesWritten,
        buffer: buffer.toString(encoding)
      };
    });
};

function writeCb(fd, buffer, offset, length, position, cb) {
  cb = cb !== undefined ? cb : arguments[arguments.length - 1];
  // follow node fs api and don't return promises
  ;(async () => {
    try {
      const { bytesWritten, buffer:outBuffer } = await write(fd, buffer, offset, length, position);
      cb(null, bytesWritten, outBuffer);
    } catch (err) {
      cb(err);
    }
  })();
}

const writeBuffer = withHooks(async (fd, buffer, offset, length, position) => {
  position = (typeof position !== 'number') ? -1 : position;
  checkFd(fd, 'node_ext2fs_write', [fd, buffer, offset, length, position]);
  const [writeBuffer, writePointer] = await useBuffer(length);
  if (typeof offset !== 'number') {
    offset = 0;
  }
  if (typeof length !== 'number') {
    length = buffer.length - offset;
  }
  if (typeof position !== 'number') {
    position = null;
  }
  buffer.copy(writeBuffer, 0, offset, offset + length);
  const bytesWritten = await binding.write(fd, openFiles.get(fd), writePointer, length, position);
  return {
    bytesWritten,
    buffer,
  }
});

const writev = async (fd, buffers, position) => {
  if (typeof position !== 'number') position = -1;

  let bytesWritten = 0;
  for (const buffer of buffers) {
    const { bytesWritten:written } = await writeBuffer(fd, buffer, 0, buffer.byteLength, position);
    bytesWritten += written;
    position > -1 && (position += bytesWritten);
  }
  return { bytesWritten, buffers };
}

async function truncate(path, len) {
  // if (typeof path === 'number') {
  //   open
  // }
  // if (typeof len === 'function') {
  //   callback = len;
  //   len = 0;
  // } else if (len === undefined) {
  //   len = 0;
  // }

  // callback = maybeCallback(callback);
  // fs.open(path, 'r+', function(er, fd) {
  //   if (er) return callback(er);
  //   var req = new FSReqWrap();
  //   req.oncomplete = function oncomplete(er) {
  //     fs.close(fd, function(er2) {
  //       callback(er || er2);
  //     });
  //   };
  //   binding.ftruncate(fd, len, req);
  // });
  throw new UnimplementedError('truncate');
};

async function ftruncate(fd, len, callback) {
  // if (typeof len === 'function') {
  //   callback = len;
  //   len = 0;
  // } else if (len === undefined) {
  //   len = 0;
  // }
  // var req = new FSReqWrap();
  // req.oncomplete = makeCallback(callback);
  // binding.ftruncate(fd, len, req);
  throw new UnimplementedError('ftruncate');
};

const rmdir = withHooks(async path => {
  path = await usePath(path);
  return await binding.rmdir(fsPointer, path);
});

const unlink = withHooks(async path => {
  path = await usePath(path);
  await binding.unlink(fsPointer, path);
});

async function fdatasync(fd, callback) {
	throw new UnimplementedError("fdatasync");;
};

async function fsync(fd, callback) {
	throw new UnimplementedError("fsync");;
};

const mkdir = withHooks(async (path, mode) => {
  mode = modeNum(mode, 0o777);
  path = await usePath(path);
  await binding.mkdir(fsPointer, path, mode);
});

const mkdtemp = withHooks(async (prefix, options) => {
  // options = getOptions(options, {});
  // if (!prefix || typeof prefix !== 'string')
  //   throw new TypeError('filename prefix is required');
  // pathCheck(prefix);
  // prefix += 'XXXXXX';
  throw new UnimplementedError('mkdtemp');;
});

const readdir = withHooks(async (path, options) => {
  options = getOptions(options, {encoding: 'utf8'});
  path = await usePath(path);
  const [entries, entriesId] = await useObject([]);
  await binding.readdir(fsPointer, path, entriesId);
  if (options.encoding === 'buffer') {
    return entries;
  }
  return entries.map((b) => b.toString(options.encoding));
});

const fstat = withHooks(async (fd) => {
  checkFd(fd, 'fstat', [fd]);
  const ctime = (await binding.stat_i_ctime(fd)) * 1000;
  // TODO(zwhitchcox) get whole structure
  return new Stats(...(await Promise.all([
    0,   // dev
    binding.stat_i_mode(fd),
    binding.stat_i_links_count(fd),
    binding.stat_i_uid(fd),
    binding.stat_i_gid(fd),
    0,  // rdev
    binding.stat_blocksize(fd),
    binding.stat_ino(fd),
    binding.stat_i_size(fd),
    binding.stat_i_blocks(fd),
    binding.stat_i_atime(fd).then(x => x * 1000),
    binding.stat_i_mtime(fd).then(x => x * 1000),
    ctime,
    ctime,
  ])));
});

async function lstat(path) {
  const fd = await open(path, O_RDONLY | O_NOFOLLOW);
  const stats = await fstat(fd);
  await close(fd);
  return stats;
};

async function stat(path) {
  const fd = await open(path, 'r', 0);
  const stats = await fstat(fd);
  await close(fd);
  return stats;
}

const readlink = withHooks(async (path, options) => {
  options = getOptions(options, {encoding: 'utf8'});
  path = await usePath(path);

  const [array, arrayId] = await useObject([]);
  await binding.readlink(fsPointer, path, arrayId);

  const [target] = array;
  const {encoding} = options;
  return encoding === 'buffer' ? target : target.toString(encoding);
});


const rename = withHooks(async (existingPath, newPath) => {
  existingPath = await usePath(existingPath);
  newPath = await usePath(newPath);
  await binding.rename(fsPointer, existingPath, newPath);
})

const link = withHooks(async (existingPath, newPath) =>
  binding.link(fsPointer, ...(await usePaths(existingPath, newPath)))
);

let symlinkWarned = false;
const symlink = withHooks(async (target, path, type) => {
  if (type && !symlinkWarned) {
    symlinkWarned = true;
    console.warn("Type is not supported on ext2fs.");
  }
  return binding.symlink(fsPointer, ...(await usePaths(target, path)));
});

const fchmod = withHooks(async (fd, mode) => {
  mode = modeNum(mode);
  checkFd(fd, 'node_ext2fs_chmod', [fd, mode]);
  await binding.chmod(fd, mode);
});

async function lchmod(path, mode) {
  const fd = await open(path, constants.O_WRONLY | constants.O_NOFOLLOW)
  try {
    await fchmod(fd, mode);
  } finally {
    await close(fd);
  }
};

async function chmod(path, mode) {
  const fd = await open(path, 0, 0);
  try {
    await fchmod(fd, modeNum(mode));
  } finally {
    await close(fd);
  }
};

async function lchown(path, uid, gid) {
  const fd = await open(path, constants.O_WRONLY | constants.O_NOFOLLOW)
  try {
    await fchown(fd, uid, gid);
  } finally {
    await close(fd);
  }
};

async function fchown(fd, uid, gid) {
  checkFd(fd, 'node_ext2fs_chown', [fd, uid, gid]);
  await binding.chown(fd, uid, gid);
};

async function chown(path, uid, gid) {
  const fd = await open(path, 0, 0);
  try {
    await fchown(fd, uid, gid);
  } finally {
    await close(fd);
  }
};

async function utimes(path, atime, mtime) {
  // atime = toUnixTimestamp(atime);
  // mtime = toUnixTimestamp(mtime);
  // pathCheck(path);
	throw new UnimplementedError('futimes');;
};

async function futimes(fd, atime, mtime) {
  // atime = toUnixTimestamp(atime);
  // mtime = toUnixTimestamp(mtime);
  throw new UnimplementedError('utimes');;
};

async function writeAll(fd, buffer, offset, length, position) {
  let bytesWritten;
  while ({ bytesWritten } = await write(fd, buffer, offset, length, position)) {
    if (bytesWritten === length)
      return;
    offset += bytesWritten;
    length -= bytesWritten;
    if (position !== null) {
      position += bytesWritten;
    }
  }
}

const kReadFileBufferLength = 8 * 1024;

async function readFile(path, options) {
  options = getOptions(options, {
    flag: 'r',
    encoding: 'utf8',
  });
  pathCheck(path);
  const fd = isFd(path) ? path : await open(path,
            stringToFlags(options.flag || 'r'),
            0o666);

  try {
    const s = await fstat(fd)
    if (!s.isFile())
      throw new Error('Not a file.');

    if (s.size > kMaxLength) {
      throw new RangeError('File size is greater than possible Buffer: ' +
                          `0x${kMaxLength.toString(16)} bytes`);
    }
    if (s.size === 0) {
      const buffers = [];
      let totalRead = 0, bytesRead;
      do {
        const buffer = Buffer.allocUnsafeSlow(kReadFileBufferLength);
        ({ bytesRead } = await read(fd, buffer, 0, kReadFileBufferLength, -1));
        totalRead += bytesRead;
        buffers.push(buffer);
      } while (bytesRead);
      return Buffer.concat(buffers, totalRead).toString();
    }

    const buffer = Buffer.allocUnsafeSlow(s.size);
    const { bytesRead } = await read(fd, buffer, 0, s.size, -1);
    return buffer.subarray(0, bytesRead).toString(options.encoding);
  } finally {
    if (path !== fd) {
      await close(fd);
    }
  }
};

async function writeFile(path, data, options) {
  const { flag, mode, encoding } = getOptions(options,
    { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const fd = isFd(path) ? fd : await open(path, flag, mode);
  try {
    const buffer = (data instanceof Buffer) ?
          data : Buffer.from(String(data), encoding || 'utf8');
    const position = /a/.test(flag) ? null : 0;

    await writeAll(fd, buffer, 0, buffer.byteLength, position);
  } finally {
    await close(fd);
  }
};

async function appendFile(path, data, options) {
  options = {
    ...getOptions(options, { encoding: 'utf8', mode: 0o666 }),
    flag: 'a',
  };

  await writeFile(path, data, options);
};

// Regexp that finds the next portion of a (partial) path
// result is [base_with_slash, base], e.g. ['somedir/', 'somedir']
const nextPartRe = /(.*?)(?:[/]+|$)/g;

// Regex to find the device root, including trailing slash. E.g. 'c:\\'.
const splitRootRe = /^[/]*/;

function encodeRealpathResult(result, options, err) {
  if (!options || !options.encoding || options.encoding === 'utf8' || err)
    return result;
  const asBuffer = Buffer.from(result);
  if (options.encoding === 'buffer') {
    return asBuffer;
  } else {
    return asBuffer.toString(options.encoding);
  }
}

// TODO(zwhitchcox): test
async function realpath(p, options) {
  options = getOptions(options);
  path = path.resolve(p.toString());
  pathCheck(p);
  while((await lstat(p).isSymbolicLink()))
    p = path.resolve(p, await readlink(p));
  const buffer = Buffer.from(p);
  return options.encoding === 'buffer' ? buffer : buffer.toString(options.encoding);
};

let pool;
function allocNewPool(poolSize) {
  pool = Buffer.allocUnsafe(poolSize);
  pool.used = 0;
}

function createReadStream(path, options) {
  return new ReadStream(path, options);
};

class ReadStream extends Readable {
  constructor (path, options) {
    super(options);
    // a little bit bigger buffer and water marks by default
    options = getOptions(options, {
      highWaterMark: 64 * 1024,
    })
    this.path = path;
    this.fd = options.fd !== undefined ? options.fd : null;
    this.flags = options.flags !== undefined ? options.flags :  'r';
    this.mode = options.mode !== undefined ? options.mode : 0o666;
    this.autoClose = options.autoClose !== undefined ? options.autoClose : true;
    this.start = options.start;
    this.bytesWritten = 0;

    if (this.start !== undefined) {
      if (typeof this.start !== 'number') {
        throw new TypeError('"start" option must be a Number');
      }
      if (this.end === undefined) {
        this.end = Infinity;
      } else if (typeof this.end !== 'number') {
        throw new TypeError('"end" option must be a Number');
      }

      if (this.start > this.end) {
        throw new Error('"start" option must be <= "end" option');
      }

      this.pos = this.start;
    }

    if (typeof this.fd !== 'number')
      this.open();

    this.on('end', function() {
      if (this.autoClose) {
        this.destroy();
      }
    });
  }

  async open() {
    try {
      const fd = await open(this.path, this.flags, this.mode);
      this.fd = fd;
      this.emit('open', fd);
      // start the flow of data.
      this.read();
    } catch (error) {
      this.emit('error', error);
      if (this.autoClose) {
        this.destroy();
      }
    }
  }

  async _read(n) {
    if (typeof this.fd !== 'number')
      return this.once('open', function() {
        this._read(n);
      });

    if (this.destroyed)
      return;

    if (!pool || pool.length - pool.used < kMinPoolSpace) {
      // discard the old pool.
      allocNewPool(this._readableState.highWaterMark);
    }

    // Grab another reference to the pool in the case that while we're
    // in the thread pool another read() finishes up the pool, and
    // allocates a new one.
    const thisPool = pool;
    const toRead = Math.min(pool.length - pool.used, n);
    const start = pool.used;

    if (this.pos !== undefined)
      toRead = Math.min(this.end - this.pos + 1, toRead);

    // already read everything we were supposed to read!
    // treat as EOF.
    if (toRead <= 0)
      return this.push(null);

    let bytesRead;
    try {
      // the actual read.
      ({bytesRead} = await read(this.fd, pool, pool.used, toRead, this.pos));
    } catch (error) {
      this.emit('error', error);
      if (this.autoClose)
        this.destroy();
      return;
    }

    if (this.pos !== undefined)
      this.pos += toRead;

    // move the pool positions, and internal position for reading.
    if (this.pos !== undefined)
      this.pos += toRead;
    pool.used += toRead;

    let b = null;
    if (bytesRead > 0) {
      this.bytesRead += bytesRead;
      b = thisPool.slice(start, start + bytesRead);
    }

    this.push(b);
  }

  destroy() {
    if (this.destroyed)
      return;
    this.destroyed = true;
    this.close();
  }

  async close(cb) {
    if (cb)
      this.once('close', cb);

    const _close = async fd => {
      try {
        await close(fd || this.fd);
        this.emit('close');
      } catch (err) {
        this.emit('error', err);
      }
      this.fd = null;
    }

    if (typeof this.fd !== 'number')
      return this.once('open', _close);
    if (this.closed) {
      return process.nextTick(() => this.emit('close'));
    }
    this.closed = true;

    _close();
  }
}

function createWriteStream(path, options) {
  return new WriteStream(path, options);
};

class WriteStream extends Writable {
  constructor(path, options) {
    super(options);
    Object.assign(this, options = getOptions(options, {
      fd: null,
      flags: 'w',
      mode: 0o666,
      autoClose: true,
      path: path,
      bytesWritten: 0,
    }));

    if (this.start !== undefined) {
      if (typeof this.start !== 'number') {
        throw new TypeError('"start" option must be a Number');
      }
      if (this.start < 0) {
        throw new Error('"start" must be >= zero');
      }

      this.pos = this.start;
    }

    if (options.encoding)
      this.setDefaultEncoding(options.encoding);

    if (typeof this.fd !== 'number')
      this.open();

    // dispose on finish.
    this.once('finish', function() {
      if (this.autoClose)
        this.close();
    });
  }

  async open() {
    try {
      const fd = await open(this.path, this.flags, this.mode);
      this.fd = fd;
      this.emit('open', fd);
    } catch (error) {
      if (this.autoClose) {
        this.destroy();
      }
      this.emit('error', error);
    }
  }

  async _write(data, encoding, cb) {
    if (!(data instanceof Buffer))
      return this.emit('error', new Error('Invalid data'));

    if (typeof this.fd !== 'number')
      return this.once('open', () => this._write(data, encoding, cb));

    try {
      const { bytesWritten } = await write(this.fd, data, 0, data.length, this.pos);
      this.bytesWritten += bytesWritten;
      cb();
    } catch (error) {
      if (this.autoClose)
        this.destroy();
      cb(error);
    }

    if (this.pos !== undefined)
      this.pos += data.length;
  }

  async _writev(data, cb) {
    if (typeof this.fd !== 'number')
      return this.once('open', function() {
        this._writev(data, cb);
      });

    const len = data.length;
    const chunks = new Array(len);
    let size = 0;

    for (let i = 0; i < len; i++) {
      const chunk = data[i].chunk;

      chunks[i] = chunk;
      size += chunk.length;
    }

    try {
      const bytesWritten = await writev(this.fd, chunks, this.pos);
      this.bytesWritten += bytesWritten;
      cb();
    } catch (error) {
      this.destroy();
      cb(error);
    }

    if (this.pos !== undefined)
      this.pos += size;
  }
}

WriteStream.prototype.destroy = ReadStream.prototype.destroy;
WriteStream.prototype.close = ReadStream.prototype.close;

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;

class FileHandle extends EventEmitter {
  constructor(fd) {
    super();
    this[kFd] = fd;
  }
  get fd() {
    return this[kFd];
  }

  close() {
    this.emit('close');
    fsCall(close, this);
    this[kFd] = -1;
  }

  appendFile(data, options) {
    // this seems wrong to me, but it's consistent with NodeJS
    return fsCall(writeFile, this, data, options);
  }

  chmod(mode) {
    return fsCall(fchmod, this, mode);
  }

  chown(uid, gid) {
    return fsCall(fchown, this, uid, gid);
  }

  datasync() {
    return fsCall(fdatasync, this);
  }

  sync() {
    return fsCall(fsync, this);
  }

  read(buffer, offset, length, position) {
    return fsCall(read, this, buffer, offset, length, position);
  }

  readv(buffers, position) {
    return fsCall(readv, this, buffers, position);
  }

  readFile(options) {
    return fsCall(readFile, this, options);
  }

  stat(options) {
    return fsCall(fstat, this, options);
  }

  truncate(len = 0) {
    return fsCall(ftruncate, this, len);
  }

  utimes(atime, mtime) {
    return fsCall(futimes, this, atime, mtime);
  }

  write(buffer, offset, length, position) {
    return fsCall(write, this, buffer, offset, length, position);
  }

  writev(buffers, position) {
    return fsCall(writev, this, buffers, position);
  }

  writeFile(data, options) {
    return fsCall(writeFile, this, data, options);
  }
}

async function fsCall(fn, handle, ...args) {
  const fd = handle[kFd];
  assert(kFd !== undefined, 'handle must be an instance of FileHandle');
  if (fd === -1) {
    const err = new Error('file closed');
    err.code = 'EBADF';
    err.syscall = fn.name;
    throw err;
  }
  return await fn(fd, ...args);
}

async function openFileHandle(path, flags, mode) {
  return new FileHandle(await open(path, flags, mode));
}

const fsPromises = {
  fsPointer,
  closeAllFileDescriptors,
  openFiles,
  constants,
  access,
  open: openFileHandle,
  close,
  read,
  write,
  readFile,
  writeFile,
  appendFile,
  rename,
  truncate,
  ftruncate,
  rmdir,
  unlink,
  fdatasync,
  fsync,
  mkdir,
  mkdtemp,
  readdir,
  fstat,
  lstat,
  stat,
  readlink,
  symlink,
  link,
  fchmod,
  lchmod,
  chmod,
  lchown,
  fchown,
  chown,
  createReadStream,
  ReadStream,
  createWriteStream,
  WriteStream,
}

const fs = {
  fsPointer,
  closeAllFileDescriptors,
  openFiles,
  constants,
  access: callbackify(access),
  open: callbackify(open),
  close: callbackify(close),
  read: readCb,
  write: writeCb,
  readFile: callbackify(readFile),
  writeFile: callbackify(writeFile),
  appendFile: callbackify(appendFile),
  rename: callbackify(rename),
  truncate: callbackify(truncate),
  ftruncate: callbackify(ftruncate),
  rmdir: callbackify(rmdir),
  unlink: callbackify(unlink),
  fdatasync: callbackify(fdatasync),
  fsync: callbackify(fsync),
  mkdir: callbackify(mkdir),
  mkdtemp: callbackify(mkdtemp),
  readdir: callbackify(readdir),
  fstat: callbackify(fstat),
  lstat: callbackify(lstat),
  stat: callbackify(stat),
  readlink: callbackify(readlink),
  symlink: callbackify(symlink),
  link: callbackify(link),
  fchmod: callbackify(fchmod),
  lchmod: callbackify(lchmod),
  chmod: callbackify(chmod),
  lchown: callbackify(lchown),
  fchown: callbackify(fchown),
  chown: callbackify(chown),
  createReadStream,
  ReadStream: callbackify(ReadStream),
  createWriteStream,
  WriteStream: callbackify(WriteStream),
}

for (const _fs of [fs, fsPromises]) {
  // Don't allow mode to accidentally be overwritten.
  ['F_OK', 'R_OK', 'W_OK', 'X_OK'].forEach(key => {
    Object.defineProperty(_fs, key, {
      enumerable: true, value: constants[key] || 0, writable: false
    });
  });
  Object.defineProperty(_fs, 'constants', {
    enumerable: true, value: constants, writable: false
  })
}

fs.promises = fsPromises;
return fs;
};