extern "C" {
	#include <ext2fs/ext2fs.h>
	#include <stdio.h>
}

#include "node_ext2fs.h"
#include "js_io.h"
#include "async.h"

using namespace Nan;

class MountWorker : public AsyncWorker {
	public:
		MountWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			request_cb = new Callback(info[0].As<v8::Function>());
		}
		~MountWorker() {}

		void Execute () {
			int open_flags = EXT2_FLAG_RW;
			int superblock = 0;
			int blocksize = 0;
			ext2_filsys fs = NULL;
			char hex_ptr[sizeof(void*) + 1];
			unsigned int start, blk;
			// ext2_file_t file;
			// ext2_ino_t ino;

			sprintf(hex_ptr, "%x", request_cb);
			ret = ext2fs_open(hex_ptr, open_flags, superblock, blocksize, &js_io_manager, &fs);
			if (ret) {
				return;
			}

			ret = ext2fs_read_inode_bitmap(fs);
			if (ret) {
				return;
			}

			ret = ext2fs_read_block_bitmap(fs);
			if (ret) {
				return;
			}

			start = fs->super->s_first_data_block;
			for (blk = start; blk <= fs->super->s_blocks_count; blk++) {
				// Check for either the last iteration or a used block
				if (blk == fs->super->s_blocks_count || ext2fs_test_block_bitmap(fs->block_map, blk)) {
					// A discard region
					if (start != blk) {
						ret = io_channel_discard(fs->io, start, blk - start);
						if (ret) {
							return;
						}
					}
					start = blk + 1;
				}
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
			HandleScope scope;

			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {
					ErrnoException(-ret)
				};
				callback->Call(1, argv);
			} else {
				v8::Local<v8::Value> argv[] = {
					Null(),
					New<v8::Number>(ret)
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
		ThrowTypeError("Wrong number of arguments");
		return;
	}

	init_async();

	if (info[1]->IsFunction()) {
		Callback *callback = new Callback(info[1].As<v8::Function>());
		AsyncQueueWorker(new MountWorker(info, callback));
	}
}
