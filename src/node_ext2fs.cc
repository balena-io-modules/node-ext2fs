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
			fs = reinterpret_cast<ext2_filsys>(
				info[0]->ToObject().As<v8::External>()->Value()
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
			if (ret) return;
			ret = ext2fs_read_bitmaps(fs);
			if (ret) return;
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
			v8::Local<v8::Value> argv[] = {Null(), New<v8::External>(fs)};
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
			fs = reinterpret_cast<ext2_filsys>(
				info[0]->ToObject().As<v8::External>()->Value()
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

ext2_ino_t string_to_inode(ext2_filsys fs, char *str) {
	ext2_ino_t ino;
	int retval;

	retval = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, str, &ino);
	if (retval) {
		com_err(str, retval, 0);
		return 0;
	}
	return ino;
}

class OpenWorker : public AsyncWorker {
	public:
		OpenWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = reinterpret_cast<ext2_filsys>(
				info[0]->ToObject().As<v8::External>()->Value()
			);
			Nan::Utf8String path_(info[1]);
			int len = strlen(*path_);
			path = malloc(len + 1);
			strncpy(path, *path_, len);
			path[len] = '\0';
			// TODO args:
			// * flags
		  	// * mode
		}
		~OpenWorker() {}

		void Execute () {
			// TODO: error handling
			ext2_ino_t ino = string_to_inode(fs, path);
			if (!ino) {  // TODO: test o_creat
				ret = ext2fs_new_inode(
					fs,
					EXT2_ROOT_INO,  // TODO extract parent dir from path
					LINUX_S_IFREG | 0600,  // TODO: mode
					0,
					&ino
				);
				if (ret) return;
			}
			ret = ext2fs_file_open(
				fs,
				ino, // inode,
				EXT2_FILE_WRITE | EXT2_FILE_CREATE, // flags TODO
				&file
			);
		}

		void HandleOKCallback () {
			HandleScope scope;
			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null(), New<v8::External>(file)};
			callback->Call(2, argv);
		}

	private:
		errcode_t ret;
		ext2_filsys fs;
		char* path;
		ext2_file_t file;
};
X_NAN_METHOD(open, OpenWorker, 5);

class CloseWorker : public AsyncWorker {
	public:
		CloseWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = reinterpret_cast<ext2_file_t>(
				info[0]->ToObject().As<v8::External>()->Value()
			);
		}
		~CloseWorker() {}

		void Execute () {
			ret = ext2fs_file_close(file);
			// TODO: free
		}

		void HandleOKCallback () {
			HandleScope scope;  // TODO: needed ?
			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null()};
			callback->Call(1, argv);
		}

	private:
		errcode_t ret;
		ext2_file_t file;
};
X_NAN_METHOD(close, CloseWorker, 2);

class ReadWorker : public AsyncWorker {
	public:
		ReadWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = reinterpret_cast<ext2_file_t>(
				info[0]->ToObject().As<v8::External>()->Value()
			);
			buffer = (char*) node::Buffer::Data(info[1]);
			offset = info[2]->IntegerValue();  // buffer offset
			length = info[3]->IntegerValue();
			position = info[4]->IntegerValue();  // file offset
		}
		~ReadWorker() {}

		void Execute () {
			// TODO: error handling
			// TODO: use llseek instead of lseek
			// TODO: loop required if length > file->fs->blocksize ?
			unsigned int pos; // needed?
			if (position != -1) {
				ret = ext2fs_file_lseek(file, position, EXT2_SEEK_SET, &pos);
			}
			ret = ext2fs_file_read(file, buffer + offset, length, &got);
		}

		void HandleOKCallback () {
			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null(), New<v8::Integer>(got)};
			callback->Call(2, argv);
		}

	private:
		errcode_t ret;
		ext2_file_t file;
		char *buffer;
		unsigned int offset;
		unsigned int length;
		unsigned int position;
		unsigned int got;
};
X_NAN_METHOD(read, ReadWorker, 6);

class WriteWorker : public AsyncWorker {
	public:
		WriteWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = reinterpret_cast<ext2_file_t>(
				info[0]->ToObject().As<v8::External>()->Value()
			);
			buffer = (char*) node::Buffer::Data(info[1]);
			offset = info[2]->IntegerValue();  // buffer offset
			length = info[3]->IntegerValue();
			position = info[4]->IntegerValue();  // file offset
		}
		~WriteWorker() {}

		void Execute () {
			// TODO: error handling
			// TODO: use llseek instead of lseek
			// TODO: loop required if length > file->fs->blocksize ?
			unsigned int pos; // needed?
			if (position != -1) {
				ret = ext2fs_file_lseek(file, position, EXT2_SEEK_SET, &pos);
			}
			ret = ext2fs_file_write(file, buffer + offset, length, &written);
		}

		void HandleOKCallback () {
			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null(), New<v8::Integer>(written)};
			callback->Call(2, argv);
		}

	private:
		errcode_t ret;
		ext2_file_t file;
		char *buffer;
		unsigned int offset;
		unsigned int length;
		unsigned int position;
		unsigned int written;
};
X_NAN_METHOD(write, WriteWorker, 6);

int copy_filename_to_result(
	struct ext2_dir_entry *dirent,
	int offset,
	int blocksize,
	char *buf,
	void *priv_data
) {
	// TODO: handle errors
	if ((strcmp(dirent->name, ".") != 0) && (strcmp(dirent->name, "..") != 0)) {
		auto filenames = static_cast<std::vector<std::string>*>(priv_data);
		filenames->push_back(dirent->name);
		return 0;
	}
}

class ReadDirWorker : public AsyncWorker {
	public:
		ReadDirWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = reinterpret_cast<ext2_filsys>(
				info[0]->ToObject().As<v8::External>()->Value()
			);
			Nan::Utf8String path_(info[1]);
			int len = strlen(*path_);
			path = malloc(len + 1);
			strncpy(path, *path_, len);
			path[len] = '\0';
		}
		~ReadDirWorker() {}

		void Execute () {
			// TODO: error handling
			ext2_ino_t ino = string_to_inode(fs, path);
			ret = ext2fs_file_open(
				fs,
				ino, // inode,
				0, // flags TODO
				&file
			);
			if (ret) return;
			ret = ext2fs_check_directory(fs, ino);
			if (ret) return;
			char *block_buf = malloc(1024);  // TODO: constant?
			ret = ext2fs_dir_iterate(
				fs,
				ino,
				0,  // flags
				block_buf,
				copy_filename_to_result,
				&filenames
			);
			//TODO: free
		}

		void HandleOKCallback () {
			if (ret < 0) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv);
				return;
			}
			v8::Local<v8::Array> result = Nan::New<v8::Array>();
			for(auto const& filename: filenames) {
				result->Set(
					result->Length(),
					Nan::CopyBuffer(
						filename.c_str(),
						filename.length()
					).ToLocalChecked()
				);
			}
			v8::Local<v8::Value> argv[] = {Null(), result};
			callback->Call(2, argv);
		}

	private:
		errcode_t ret;
		ext2_filsys fs;
		char* path;
		ext2_file_t file;
		std::vector<std::string> filenames;
};
X_NAN_METHOD(readdir, ReadDirWorker, 3);

v8::Local<v8::Value> castUint32(long unsigned int x) {
	return Nan::New<v8::Uint32>(static_cast<uint32_t>(x));
}

v8::Local<v8::Value> castInt32(long int x) {
	return Nan::New<v8::Int32>(static_cast<int32_t>(x));
}

v8::Local<v8::Value> timespecToMilliseconds(__u32 seconds) {
    return Nan::New<v8::Number>(static_cast<double>(seconds) * 1000);
}

NAN_METHOD(fstat) {
	CHECK_ARGS(3)
	auto file = reinterpret_cast<ext2_file_t>(
		info[0]->ToObject().As<v8::External>()->Value()
	);
	auto Stats = info[1].As<v8::Function>();
	auto *callback = new Callback(info[2].As<v8::Function>());
	v8::Local<v8::Value> statsArgs[] = {
		castInt32(0),   // dev
		castInt32(file->inode.i_mode),
		castInt32(file->inode.i_links_count),
		castInt32(file->inode.i_uid),
		castUint32(file->inode.i_gid),
		castUint32(0),  // rdev
		castInt32(file->fs->blocksize),
		castInt32(file->ino),
		castUint32(file->inode.i_size),
		castUint32(file->inode.i_blocks),
		timespecToMilliseconds(file->inode.i_atime),
		timespecToMilliseconds(file->inode.i_mtime),
		timespecToMilliseconds(file->inode.i_ctime),
		timespecToMilliseconds(file->inode.i_ctime),
	};
	auto stats = Nan::NewInstance(Stats, 14, statsArgs).ToLocalChecked();
	v8::Local<v8::Value> argv[] = {Null(), stats};
	callback->Call(2, argv);
}

NAN_METHOD(init) {
	init_async();
}

NAN_METHOD(closeExt) {
	close_async();
}
