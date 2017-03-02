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

class TrimWorker : public AsyncWorker {
	public:
		TrimWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			auto filesystem = info[0]->ToObject();

			auto fs_key = New<v8::String>("fs").ToLocalChecked();
			auto request_cb_key = New<v8::String>("request_cb").ToLocalChecked();
			fs = reinterpret_cast<ext2_filsys>(filesystem->Get(fs_key).As<v8::External>()->Value());
			request_cb = reinterpret_cast<Callback*>(filesystem->Get(request_cb_key).As<v8::External>()->Value());
		}
		~TrimWorker() {}

		void Execute () {
			unsigned int start, blk, count;

			if (!fs->block_map) {
				if ((ret = ext2fs_read_block_bitmap(fs))) {
					return;
				}
			}

			start = fs->super->s_first_data_block;
			count = fs->super->s_blocks_count;
			for (blk = start; blk <= count; blk++) {
				// Check for either the last iteration or a used block
				if (blk == count || ext2fs_test_block_bitmap(fs->block_map, blk)) {
					if (start < blk) {
						if ((ret = io_channel_discard(fs->io, start, blk - start))) {
							return;
						}
					}
					start = blk + 1;
				}
			}
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
				};
				callback->Call(1, argv);
			}
		}

	private:
		errcode_t ret;
		Callback *request_cb;
		ext2_filsys fs;

};

NAN_METHOD(trim) {
	if (info.Length() != 2) {
		ThrowTypeError("Wrong number of arguments");
		return;
	}

	if (info[1]->IsFunction()) {
		Callback *callback = new Callback(info[1].As<v8::Function>());
		AsyncQueueWorker(new TrimWorker(info, callback));
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
			char hex_ptr[sizeof(void*) * 2 + 3];

			sprintf(hex_ptr, "%p", request_cb);
			ret = ext2fs_open(hex_ptr, open_flags, 0, 0, get_js_io_manager(), &fs);
			if (ret) {
				return;
			}
		}

		void HandleOKCallback () {
			HandleScope scope;

			// FIXME: when V8 garbage collects this object we should also free
			// any resources allocated by libext2fs
			auto filesystem = New<v8::Object>();
			filesystem->Set(
				New<v8::String>("fs").ToLocalChecked(),
				New<v8::External>(fs)
			);
			filesystem->Set(
				New<v8::String>("request_cb").ToLocalChecked(),
				New<v8::External>(request_cb)
			);

			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {
					ErrnoException(-ret)
				};
				callback->Call(1, argv);
			} else {
				v8::Local<v8::Value> argv[] = {
					Null(),
					filesystem
				};
				callback->Call(2, argv);
			}
		}

	private:
		errcode_t ret;
		ext2_filsys fs;
		Callback *request_cb;
};

NAN_METHOD(mount) {
	if (info.Length() != 2) {
		ThrowTypeError("Wrong number of arguments");
		return;
	}

	if (info[1]->IsFunction()) {
		Callback *callback = new Callback(info[1].As<v8::Function>());
		AsyncQueueWorker(new MountWorker(info, callback));
	}
}

NAN_METHOD(init) {
	init_async();
}
