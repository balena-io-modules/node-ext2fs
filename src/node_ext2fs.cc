extern "C" {
	#include <ext2fs/ext2fs.h>
	#include <stdio.h>
}

#include "node_ext2fs.h"
#include "js_io.h"
#include "async.h"

// name2handle = map();

// NAN_METHOD(mount) {
// 	name2handle[name] = { open, close, read, write, flush };
// 
// 	ret = ext2fs_open(handle, open_flags, superblock, blocksize, js_io_manager, &fs);
// }

class MountWorker : public Nan::AsyncWorker {
	public:
		MountWorker(Nan::NAN_METHOD_ARGS_TYPE info, Nan::Callback *callback)
		: Nan::AsyncWorker(callback) {
			request_cb = new Nan::Callback(info[0].As<v8::Function>());
		}
		~MountWorker() {}

		void Execute () {
			int open_flags = EXT2_FLAG_RW;
			int superblock = 0;
			int blocksize = 0;
			ext2_filsys fs = NULL;
			ext2_file_t file;
			ext2_ino_t ino;
			char abomination[17];

			sprintf(abomination, "%x", request_cb);
			ret = ext2fs_open(abomination, open_flags, superblock, blocksize, &js_io_manager, &fs);
			if (ret) {
				fprintf(stderr, "%s: failed to open filesystem\n", "testlib");
				return;
			}

			// printf("trying to find inode\n");
			// ret = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, file_path, &ino);
			// if (ret) {
			// 	fprintf(stderr, "%s: failed to lookup file %s\n", "testlib", file_path);
			// 	return;
			// }

			// printf("opening file with ino=%d\n", ino);
			// ret = ext2fs_file_open(fs, ino, EXT2_FILE_WRITE, &file);
			// if (ret) {
			// 	fprintf(stderr, "%s: failed to open inode %s\n", "testlib", ino);
			// 	return;
			// }
		}

		void HandleOKCallback () {
			Nan::HandleScope scope;

			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {
					Nan::ErrnoException(-ret)
				};
				callback->Call(1, argv);
			} else {
				v8::Local<v8::Value> argv[] = {
					Nan::Null(),
					Nan::New<v8::Number>(ret)
				};
				callback->Call(2, argv);
			}
		}

	private:
		errcode_t ret;
		Callback *request_cb;
};

NAN_METHOD(mount) {
	if (info.Length() != 2) {
		Nan::ThrowTypeError("Wrong number of arguments");
		return;
	}

	init_async();

	if (info[1]->IsFunction()) {
		Nan::Callback *callback = new Nan::Callback(info[1].As<v8::Function>());
		Nan::AsyncQueueWorker(new MountWorker(info, callback));
	}
}
