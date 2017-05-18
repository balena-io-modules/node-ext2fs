#include <stdio.h>
#include <nan.h>

#include "ext2fs.h"
#include "js_io.h"
#include "async.h"

#include "node_ext2fs.h"

#define CHECK_ARGS(length) \
	if (info.Length() != length) { \
		ThrowTypeError("Wrong number of arguments"); \
		return; \
	} \
	if (!info[length - 1]->IsFunction()) { \
		ThrowTypeError("A callback is required"); \
		return; \
	}

#define X_NAN_METHOD(name, worker, nb_args) \
	NAN_METHOD(name) { \
		CHECK_ARGS(nb_args) \
		Callback *callback = new Callback(info[nb_args - 1].As<v8::Function>()); \
		AsyncQueueWorker(new worker(info, callback)); \
	}


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
			request_cb = reinterpret_cast<Callback*>(
				filesystem->Get(request_cb_key).As<v8::External>()->Value()
			);
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

X_NAN_METHOD(trim, TrimWorker, 2);

class MountWorker : public AsyncWorker {
	public:
		MountWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback) : AsyncWorker(callback) {
			request_cb = new Callback(info[0].As<v8::Function>());
		}
		~MountWorker() {}

		void Execute () {
			char hex_ptr[sizeof(void*) * 2 + 3];
			sprintf(hex_ptr, "%p", request_cb);
			ret = ext2fs_open(
				hex_ptr,              // name
				EXT2_FLAG_RW,         // flags
				0,                    // superblock
				0,                    // block_size
				get_js_io_manager(),  // manager
				&fs                   // ret_fs
			);
		}

		void HandleOKCallback () {
			HandleScope scope;
			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv);
				return;
			}
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
			v8::Local<v8::Value> argv[] = {Null(), filesystem};
			callback->Call(2, argv);
		}

	private:
		errcode_t ret;
		ext2_filsys fs;
		Callback *request_cb;
};

X_NAN_METHOD(mount, MountWorker, 2);

class UmountWorker : public AsyncWorker {
	public:
		UmountWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			auto filesystem = info[0]->ToObject();
			auto fs_key = New<v8::String>("fs").ToLocalChecked();
			fs = reinterpret_cast<ext2_filsys>(
				filesystem->Get(fs_key).As<v8::External>()->Value()
			);
		}
		~UmountWorker() {}

		void Execute () {
			ret = ext2fs_close(fs);
		}

		void HandleOKCallback () {
			v8::Local<v8::Value> argv[1];
			if (ret < 0) {
				argv[0] = ErrnoException(-ret);
			} else {
				argv[0] = Null();
			}
			callback->Call(1, argv);
		}

	private:
		errcode_t ret;
		ext2_filsys fs;
};

X_NAN_METHOD(umount, UmountWorker, 2);

NAN_METHOD(init) {
	init_async();
}

NAN_METHOD(close) {
	close_async();
}
