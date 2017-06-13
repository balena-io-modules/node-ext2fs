node-ext2fs
=========
### NodeJS native bindings to the linux ext{2,3,4} filesystem library
[![Build Status](https://travis-ci.org/resin-io/node-ext2fs.svg?branch=master)](https://travis-ci.org/resin-io/node-ext2fs)

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
You can also issue `DISCARD` requests using `ext2fs.trim(filesystem, callback)`

See the examples below.

Example
-------

```javascript
const ext2fs = require('ext2fs');
const filedisk = require('file-disk');
const fs = require('fs');

const fd = fs.openSync('/path/to/ext4_filesystem.img', 'r+');
const disk = new filedisk.FileDisk(fd);

ext2fs.mount(disk, function(err, filesystem) {
	if (err) {
		return;
	}
	// filesystem behaves like node's fs
	console.log('Mounted filesystem successfully');
	filesystem.readFile('/some_file', 'utf8', function(err, contents) {
		if (err) {
			return;
		}
		console.log('contents:', contents);
		ext2fs.trim(filesystem, function(err) {
			if (err) {
				return;
			}
			console.log('TRIMed filesystem');
			// don't forget to umount
			ext2fs.umount(filesystem, function(err) {
				if (err) {
					return;
				}
				console.log('filesystem umounted')
				fs.closeSync(fd)
				// Call ext2fs.close to release allocated resources and let node exit.
				ext2fs.close()
			});
		});
	});
});

```

Example using promises
----------------------

The code above isn't very practical as it requires a new level of indentation
for each call. Let's simplify it using promises:

```javascript
const Promise = require('bluebird')
const ext2fs = Promise.promisifyAll(require('ext2fs'));
const filedisk = require('file-disk');

Promise.using(filedisk.openFile(path, 'r+'), function(fd) {
	const disk = new filedisk.FileDisk(fd);
	return ext2fs.mountAsync(disk)
	.then(function(filesystem) {
		filesystem = Promise.promisifyAll(filesystem);
		// filesystem behaves like node's fs
		console.log('Mounted filesystem successfully');
		return filesystem.readFileAsync('/some_file', 'utf8')
		.then(function(contents) {
			console.log('contents:', contents);
			return ext2fs.trimAsync(filesystem);
		})
		.then(function() {
			console.log('TRIMed filesystem');
			// don't forget to umount
			return ext2fs.umountAsync(filesystem);
		})
		.then(function() {
			console.log('filesystem umounted');
		});
	});
})
.then(function() {
	// Call ext2fs.close to release allocated resources and let node exit.
	return ext2fs.closeAsync();
});

```

Support
-------

If you're having any problems, please [raise an issue][github-issue] on GitHub.

License
-------

node-ext2fs is free software, and may be redistributed under the terms specified
in the [license].

[github-issue]: https://github.com/resin-io/node-ext2fs/issues/new
[license]: https://github.com/resin-io/node-ext2fs/blob/master/LICENSE
