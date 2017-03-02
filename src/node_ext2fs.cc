#include <stdio.h>
#include <nan.h>

#include "ext2fs.h"
#include "js_io.h"
#include "async.h"

#include "node_ext2fs.h"

using namespace Nan;

extern "C" {
void com_err(const char *a, long b, const char *c, ...) {
	return;
}
}

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
			char hex_ptr[sizeof(void*) * 2 + 3];
			unsigned int start, blk;

			sprintf(hex_ptr, "%p", request_cb);
			ret = ext2fs_open(hex_ptr, open_flags, superblock, blocksize, get_js_io_manager(), &fs);
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

			// start = fs->super->s_first_data_block;
			// for (blk = start; blk <= fs->super->s_blocks_count; blk++) {
			// 	// Check for either the last iteration or a used block
			// 	if (blk == fs->super->s_blocks_count || ext2fs_test_block_bitmap(fs->block_map, blk)) {
			// 		// A discard region
			// 		if (start != blk) {
			// 			ret = io_channel_discard(fs->io, start, blk - start);
			// 			if (ret) {
			// 				return;
			// 			}
			// 		}
			// 		start = blk + 1;
			// 	}
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
