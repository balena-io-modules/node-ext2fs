node-ext2fs
=========
### WASM bindings to the linux ext{2,3,4} filesystem library

node-ext2fs uses the [e2fsprogs](https://github.com/tytso/e2fsprogs) project to
provide access to ext filesystem from javascript.

The `node-` in `node-ext2fs` is here because it was a native node module until `v3.0.0` (excluded).
Since `v3.0.0`, it is a WebAssembly module built with [emscripten](https://emscripten.org/).

Some things you can do with this module:

* Read/write files in a filesystem image directly without mounting
* Use familiar APIs, node-ext2fs has the exact same interface as node's `fs` module
* Combine node-ext2fs filesystem streams with host filesystem streams (e.g copy files)
* Create a tar archive from a filesystem image
* Perform a TRIM operation to obtain discard regions of a filesystem

Installation
------------

Simply install `node-ext2fs` using `npm`:

``` bash
$ npm install node-ext2fs
```

Building
--------

Prerequisites:

* git
* make
* [`emcc` >= 2.0.7](https://emscripten.org/docs/getting_started/downloads.html)

```
git clone --recursive https://github.com/balena-io-modules/node-ext2fs # clone the repository
cd  node-ext2fs
npm i
npm run build
```

Usage
-----

Mount a disk image and use the returned `fs` object.
The fs returned object behaves like node's `fs` except it doesn't provide any
xxxxSync method.
You can also issue `DISCARD` requests using the fs `async trim()` method.

See the example below.

Example
-------

```javascript
const { withMountedDisk } = require('ext2fs');
const { FileDisk, withOpenFile } = require('file-disk');
const { promisify } = require('util');

async function main() {
  const diskImage = '/some/disk.image';
  const offset = 272629760;  // offset of the ext partition you want to mount in that disk image
  try {
    await withOpenFile(diskImage, 'r', async (handle) => {
      const disk = new FileDisk(handle);
      await withMountedDisk(disk, offset, async ({promises:fs}) => {
        // List files
        console.log('readdir', await fs.readdir('/'));
        await fs.trim();
        // Show discarded regions
        console.log('discarded', disk.getDiscardedChunks());
        // Show ranges of useful data aligned to 1MiB
        console.log('ranges', await disk.getRanges(1024 ** 2));
      });
    });
  } catch (error) {
    console.error(error);
  }
}

```

Support
-------

If you're having any problems, please [raise an issue][github-issue] on GitHub.

License
-------

node-ext2fs is free software, and may be redistributed under the terms specified
in the [license].

[github-issue]: https://github.com/balena-io/node-ext2fs/issues/new
[license]: https://github.com/balena-io/node-ext2fs/blob/master/LICENSE
