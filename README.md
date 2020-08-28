node-ext2fs
=========
### NodeJS native bindings to the linux ext{2,3,4} filesystem library
[![Build Status](https://travis-ci.org/balena-io/node-ext2fs.svg?branch=master)](https://travis-ci.org/balena-io/node-ext2fs)

node-ext2fs uses the [e2fsprogs](https://github.com/tytso/e2fsprogs) project to
provide access to ext filesystem from NodeJS in a cross-platform way.

Some things you can do with this module:

* Read/write files in a filesystem image directly without mounting
* Use familiar APIs, node-ext2fs has the exact same interface as node's `fs` module
* Combine node-ext2fs filesystem streams with host filesystem streams (e.g copy files)
* Create a tar archive from a filesystem image
* Perform a TRIM operation to obtain discard regions of a filesystem

**Warning: The API exposed by this library is still forming and can change at
any time!**

Installation
------------

To install `node-ext2fs` you need to have gcc and make available to your
environment. For Linux and Mac having a working node-gyp installation is
enough. To install on windows, you have to install MingW64 and make sure
`mingw32-make` and `gcc` are available in your Powershell or cmd.exe terminal.

Simply compile and install `node-ext2fs` using `npm`:

``` bash
$ npm install ext2fs
```

Usage
-----

Mount a disk image and use the returned `fs` object.
The fs returned object behaves like node's `fs` except it doesn't provide any
xxxxSync method.
You can also issue `DISCARD` requests using the fs `trim(callback)` method.

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
                        await withMountedDisk(disk, { offset }, async (fs) => {
                                const trim = promisify(fs.trim);
                                const readdir = promisify(fs.readdir);
				// List files
                                console.log('readdir', await readdir('/'));
                                await trim();
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

### Package fails to install as no pre-built package is available

Node-ext2fs is pre-built for a range of OSs and Node versions, but we don't have perfect coverage
here yet, and it may fail to install if you're not on an pre-built version and you don't have local
build tools available.

If you have an issue with this, and your platform is one you feel we should support, please
[raise an issue][github-issue] on this repo, so we can look at adding your configuration to the
pre-built versions that works automatically.

In the meantime, you can typically install this package by updating to a newer Node release which
does have pre-built binaries, or by setting up a local environment so the build is successful (see
['Installation'](#installation) above).

License
-------

node-ext2fs is free software, and may be redistributed under the terms specified
in the [license].

[github-issue]: https://github.com/balena-io/node-ext2fs/issues/new
[license]: https://github.com/balena-io/node-ext2fs/blob/master/LICENSE
