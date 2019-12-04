#include <stdio.h>
#include <nan.h>
#include <ctime>

#include "ext2fs.h"
#include "js_io.h"
#include "async.h"

#include "node_ext2fs.h"

#define O_DIRECTORY 0200000
#define O_NOATIME 04000000

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

ext2_filsys get_filesystem(NAN_METHOD_ARGS_TYPE info) {
	return static_cast<ext2_filsys>(
		Nan::To<v8::Object>(info[0]).ToLocalChecked().As<v8::External>()->Value()
	);
}

ext2_file_t get_file(NAN_METHOD_ARGS_TYPE info) {
	return static_cast<ext2_file_t>(
		Nan::To<v8::Object>(info[0]).ToLocalChecked().As<v8::External>()->Value()
	);
}

unsigned int get_flags(NAN_METHOD_ARGS_TYPE info) {
	return static_cast<unsigned int>(
		Nan::To<int64_t>(info[1]).FromJust()
	);
}

std::string get_path(NAN_METHOD_ARGS_TYPE info) {
	Nan::Utf8String path_(info[1]);
	return std::string(static_cast<char*>(*path_));
}

class TrimWorker : public AsyncWorker {
	public:
		TrimWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = get_filesystem(info);
		}

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

			if (ret) {
				v8::Local<v8::Value> argv[] = {
					ErrnoException(-ret)
				};
				callback->Call(1, argv, async_resource);
			} else {
				v8::Local<v8::Value> argv[] = {
					Null(),
				};
				callback->Call(1, argv, async_resource);
			}
		}

	private:
		errcode_t ret = 0;
		ext2_filsys fs;

};
X_NAN_METHOD(trim, TrimWorker, 2);

class MountWorker : public AsyncWorker {
	public:
		MountWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback) : AsyncWorker(callback) {
			// request_cb and fs will be freed in UmountWorker
			request_cb = new Callback(info[0].As<v8::Function>());
		}

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
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			// FIXME: when V8 garbage collects this object we should also free
			// any resources allocated by libext2fs
			v8::Local<v8::Value> argv[] = {Null(), New<v8::External>(fs)};
			callback->Call(2, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_filsys fs;
		Callback *request_cb;
};
X_NAN_METHOD(mount, MountWorker, 2);

class UmountWorker : public AsyncWorker {
	public:
		UmountWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = get_filesystem(info);
		}

		void Execute () {
			Callback* request_cb;
			sscanf(fs->device_name, "%p", &request_cb);
			ret = ext2fs_close_free(&fs);
			delete request_cb;
		}

		void HandleOKCallback () {
			v8::Local<v8::Value> argv[1];
			if (ret) {
				argv[0] = ErrnoException(-ret);
			} else {
				argv[0] = Null();
			}
			callback->Call(1, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_filsys fs;
};
X_NAN_METHOD(umount, UmountWorker, 2);

ext2_ino_t string_to_inode(ext2_filsys fs, const char *str) {
	ext2_ino_t ino;
	int retval;

	retval = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, str, &ino);
	if (retval) {
		com_err(str, retval, 0);
		return 0;
	}
	return ino;
}

ext2_ino_t get_parent_dir_ino(ext2_filsys fs, char* path) {
	char* last_slash = strrchr(path, (int)'/');
	if (last_slash == NULL) {
		return NULL;
	}
	unsigned int parent_len = last_slash - path + 1;
	char* parent_path = new char[parent_len + 1];
	strncpy(parent_path, path, parent_len);
	parent_path[parent_len] = '\0';
	ext2_ino_t parent_ino = string_to_inode(fs, parent_path);
	delete[] parent_path;
	return parent_ino;
}

char* get_filename(const char* path) {
	char* last_slash = strrchr((char*)path, (int)'/');
	if (last_slash == NULL) {
		return NULL;
	}
	char* filename = last_slash + 1;
	if (strlen(filename) == 0) {
		return NULL;
	}
	return filename;
}

unsigned int translate_open_flags(unsigned int js_flags) {
	unsigned int result = 0;
	if (js_flags & (O_WRONLY | O_RDWR)) {
		result |= EXT2_FILE_WRITE;
	}
	if (js_flags & O_CREAT) {
		result |= EXT2_FILE_CREATE;
	}
//	JS flags:
//	O_RDONLY
//	O_WRONLY		EXT2_FILE_WRITE
//	O_RDWR
//	O_CREAT			EXT2_FILE_CREATE
//	O_EXCL
//	O_NOCTTY
//	O_TRUNC
//	O_APPEND
//	O_DIRECTORY
//	O_NOATIME
//	O_NOFOLLOW
//	O_SYNC
//	O_SYMLINK
//	O_DIRECT
//	O_NONBLOCK
	return result;
}

errcode_t create_file(ext2_filsys fs, const char* path, unsigned int mode, ext2_ino_t* ino) {
	errcode_t ret = 0;
	ext2_ino_t parent_ino = get_parent_dir_ino(fs, (char*)path);
	if (parent_ino == NULL) {
		return -ENOTDIR;
	}
	ret = ext2fs_new_inode(fs, parent_ino, mode, 0, ino);
	if (ret) return ret;
	char* filename = get_filename(path);
	if (filename == NULL) {
		// This should never happen.
		return -EISDIR;
	}
	ret = ext2fs_link(fs, parent_ino, filename, *ino, EXT2_FT_REG_FILE);
	if (ret == EXT2_ET_DIR_NO_SPACE) {
		ret = ext2fs_expand_dir(fs, parent_ino);
		if (ret) return ret;
		ret = ext2fs_link(fs, parent_ino, filename, *ino, EXT2_FT_REG_FILE);
	}
	if (ret) return ret;
	if (ext2fs_test_inode_bitmap2(fs->inode_map, *ino)) {
		printf("Warning: inode already set\n");
	}
	ext2fs_inode_alloc_stats2(fs, *ino, +1, 0);
	struct ext2_inode inode;
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = (mode & ~LINUX_S_IFMT) | LINUX_S_IFREG;
	inode.i_atime = inode.i_ctime = inode.i_mtime = static_cast<__u32>(time(0));
	inode.i_links_count = 1;
	ret = ext2fs_inode_size_set(fs, &inode, 0);  // TODO: udpate size? also on write?
	if (ret) return ret;
	if (ext2fs_has_feature_inline_data(fs->super)) {
		inode.i_flags |= EXT4_INLINE_DATA_FL;
	} else if (ext2fs_has_feature_extents(fs->super)) {
		ext2_extent_handle_t handle;
		inode.i_flags &= ~EXT4_EXTENTS_FL;
		ret = ext2fs_extent_open2(fs, *ino, &inode, &handle);
		if (ret) return ret;
		ext2fs_extent_free(handle);
	}

	ret = ext2fs_write_new_inode(fs, *ino, &inode);
	if (ret) return ret;
	if (inode.i_flags & EXT4_INLINE_DATA_FL) {
		ret = ext2fs_inline_data_init(fs, *ino);
		if (ret) return ret;
	}
	return 0;
}

class OpenWorker : public AsyncWorker {
	public:
		OpenWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = get_filesystem(info);
			path = get_path(info);
			flags = static_cast<unsigned int>(Nan::To<int64_t>(info[2]).FromJust());
			mode = static_cast<unsigned int>(Nan::To<int64_t>(info[3]).FromJust());
		}

		void Execute () {
			// TODO: O_NOFOLLOW, O_SYMLINK
			ext2_ino_t ino = string_to_inode(fs, path.c_str());
			if (!ino) {
				if (!(flags & O_CREAT)) {
					ret = -ENOENT;
					return;
				}
				ret = create_file(fs, path.c_str(), mode, &ino);
				if (ret) return;
			} else if (flags & O_EXCL) {
				ret = -EEXIST;
				return;
			}
			if ((flags & O_DIRECTORY) && ext2fs_check_directory(fs, ino)) {
				ret = -ENOTDIR;
				return;
			}
			ret = ext2fs_file_open(fs, ino, translate_open_flags(flags), &file);
			if (ret) return;
			if (flags & O_TRUNC) {
				ret = ext2fs_file_set_size2(file, 0);
			}
		}

		void HandleOKCallback () {
			HandleScope scope;
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null(), New<v8::External>(file)};
			callback->Call(2, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_filsys fs;
		std::string path;
		unsigned int flags;
		unsigned int mode;
		ext2_file_t file;
};
X_NAN_METHOD(open, OpenWorker, 5);

class CloseWorker : public AsyncWorker {
	public:
		CloseWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = get_file(info);
			flags = get_flags(info);
		}

		void Execute () {
			ret = ext2fs_file_close(file);
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null()};
			callback->Call(1, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_file_t file;
		unsigned int flags;
};
X_NAN_METHOD(close, CloseWorker, 3);

#ifdef _WIN32
// Windows does not include CLOCK_REALTIME or clock_gettime
#define CLOCK_REALTIME 0
int clock_gettime(int, struct timespec *spec) {
	__int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime);
	wintime      -= 116444736000000000i64;  //1jan1601 to 1jan1970
	spec->tv_sec  = wintime / 10000000i64;           //seconds
	spec->tv_nsec = wintime % 10000000i64 *100;      //nano-seconds
	return 0;
}
#endif

#if defined(__MACH__) && !defined(CLOCK_REALTIME)
// clock_gettime is not implemented on older versions of OS X (< 10.12).
// If implemented, CLOCK_REALTIME will have already been defined.

#include <sys/time.h>
#define CLOCK_REALTIME 0
int clock_gettime(int, struct timespec* spec) {
		struct timeval now;
		int rv = gettimeofday(&now, NULL);
		if (rv) return rv;
		spec->tv_sec  = now.tv_sec;
		spec->tv_nsec = now.tv_usec * 1000;
		return 0;
}
#endif

static int update_xtime(ext2_file_t file, bool a, bool c, bool m) {
	errcode_t err = 0;
	err = ext2fs_read_inode(file->fs, file->ino, &(file->inode));
	if (err) return err;
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	if (a) {
		file->inode.i_atime = static_cast<__u32>(now.tv_sec);
	}
	if (c) {
		file->inode.i_ctime = static_cast<__u32>(now.tv_sec);
	}
	if (m) {
		file->inode.i_mtime = static_cast<__u32>(now.tv_sec);
	}
	increment_version(&(file->inode));
	err = ext2fs_write_inode(file->fs, file->ino, &(file->inode));
	return err;
}

class ReadWorker : public AsyncWorker {
	public:
		ReadWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = get_file(info);
			flags = get_flags(info);
			buffer = (char*) node::Buffer::Data(info[2]);
			offset = static_cast<unsigned long long>(Nan::To<int64_t>(info[3]).FromJust());  // buffer offset
			length = static_cast<unsigned int>(Nan::To<int64_t>(info[4]).FromJust());
			position = static_cast<unsigned long long>(Nan::To<int64_t>(info[5]).FromJust());  // file offset
		}

		void Execute () {
			if ((flags & O_WRONLY) != 0) {
				// Don't try to read write only files.
				ret = -EBADF;
				return;
			}
			if (position != -1) {
				ret = ext2fs_file_llseek(file, position, EXT2_SEEK_SET, NULL);
				if (ret) return;
			}
			ret = ext2fs_file_read(file, buffer + offset, length, &got);
			if (ret) return;
			if ((flags & O_NOATIME) == 0) {
				ret = update_xtime(file, true, false, false);
			}
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null(), New<v8::Integer>(got)};
			callback->Call(2, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_file_t file;
		unsigned int flags;
		char *buffer;
		unsigned long long offset;
		unsigned long long length;
		int position;
		unsigned int got;
};
X_NAN_METHOD(read, ReadWorker, 7);

class WriteWorker : public AsyncWorker {
	public:
		WriteWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = get_file(info);
			flags = get_flags(info);
			buffer = static_cast<char*>(node::Buffer::Data(info[2]));
			offset = static_cast<unsigned long long>(Nan::To<int64_t>(info[3]).FromJust());  // buffer offset
			length = static_cast<unsigned int>(Nan::To<int64_t>(info[4]).FromJust());
			position = static_cast<unsigned long long>(Nan::To<int64_t>(info[5]).FromJust());  // file offset
		}

		void Execute () {
			if ((flags & (O_WRONLY | O_RDWR)) == 0) {
				// Don't try to write to readonly files.
				ret = -EBADF;
				return;
			}
			if ((flags & O_APPEND) != 0) {
				// append mode: seek to the end before each write
				ret = ext2fs_file_llseek(file, NULL, EXT2_SEEK_END, NULL);
			} else if (position != -1) {
				ret = ext2fs_file_llseek(file, position, EXT2_SEEK_SET, NULL);
			}
			if (ret) return;
			ret = ext2fs_file_write(file, buffer + offset, length, &written);
			if (ret) return;
			if ((flags & O_CREAT) != 0) {
				ret = update_xtime(file, false, true, true);
			}
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null(), New<v8::Integer>(written)};
			callback->Call(2, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_file_t file;
		unsigned int flags;
		char *buffer;
		unsigned long long offset;
		unsigned long long length;
		int position;
		unsigned int written;
};
X_NAN_METHOD(write, WriteWorker, 7);

class ChModWorker : public AsyncWorker {
	public:
		ChModWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = get_file(info);
			flags = get_flags(info);
			mode = static_cast<unsigned int>(Nan::To<int64_t>(info[2]).FromJust());
		}

		void Execute () {
			ret = ext2fs_read_inode(file->fs, file->ino, &(file->inode));
			if (ret) return;
			// keep only fmt (file or directory)
			file->inode.i_mode &= LINUX_S_IFMT;
			// apply new mode
			file->inode.i_mode |= (mode & ~LINUX_S_IFMT);
			increment_version(&(file->inode));
			ret = ext2fs_write_inode(file->fs, file->ino, &(file->inode));
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null()};
			callback->Call(1, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_file_t file;
		unsigned int flags;
		unsigned int mode;
};
X_NAN_METHOD(fchmod, ChModWorker, 4);

class ChOwnWorker : public AsyncWorker {
	public:
		ChOwnWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			file = get_file(info);
			flags = get_flags(info);
			uid = static_cast<unsigned int>(Nan::To<int64_t>(info[2]).FromJust());
			gid = static_cast<unsigned int>(Nan::To<int64_t>(info[3]).FromJust());
		}

		void Execute () {
			// TODO handle 32 bit {u,g}ids
			ret = ext2fs_read_inode(file->fs, file->ino, &(file->inode));
			if (ret) return;
			// keep only the lower 16 bits
			file->inode.i_uid = uid & 0xFFFF;
			file->inode.i_gid = gid & 0xFFFF;
			increment_version(&(file->inode));
			ret = ext2fs_write_inode(file->fs, file->ino, &(file->inode));
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null()};
			callback->Call(1, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_file_t file;
		unsigned int flags;
		unsigned int uid;
		unsigned int gid;
};
X_NAN_METHOD(fchown, ChOwnWorker, 5);

int copy_filename_to_result(
	struct ext2_dir_entry *dirent,
	int offset,
	int blocksize,
	char *buf,
	void *priv_data
) {
	size_t len = ext2fs_dirent_name_len(dirent);
	if (
		(strncmp(dirent->name, ".", len) != 0) &&
		(strncmp(dirent->name, "..", len) != 0)
	) {
		auto filenames = static_cast<std::vector<std::string>*>(priv_data);
		filenames->push_back(std::string(dirent->name, len));
	}
	return 0;
}

class ReadDirWorker : public AsyncWorker {
	public:
		ReadDirWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = get_filesystem(info);
			path = get_path(info);
		}

		void Execute () {
			ext2_ino_t ino = string_to_inode(fs, path.c_str());
			if (ino == 0) {
				ret = -ENOENT;
				return;
			}
			ret = ext2fs_file_open(
				fs,
				ino, // inode,
				0, // flags TODO
				&file
			);
			if (ret) return;
			ret = ext2fs_check_directory(fs, ino);
			if (ret) return;
			char* block_buf = new char[fs->blocksize];
			ret = ext2fs_dir_iterate(
				fs,
				ino,
				0,  // flags
				block_buf,
				copy_filename_to_result,
				&filenames
			);
			delete[] block_buf;
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Array> result = Nan::New<v8::Array>();
			auto context = Nan::GetCurrentContext();
			for(auto const& filename: filenames) {
				result->Set(
					context,
					result->Length(),
					Nan::CopyBuffer(
						filename.c_str(),
						filename.length()
					).ToLocalChecked()
				);
			}
			v8::Local<v8::Value> argv[] = {Null(), result};
			callback->Call(2, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_filsys fs;
		std::string path;
		ext2_file_t file;
		std::vector<std::string> filenames;
};
X_NAN_METHOD(readdir, ReadDirWorker, 3);

class UnlinkWorker : public AsyncWorker {
	public:
		UnlinkWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = get_filesystem(info);
			path = get_path(info);
			rmdir = false;
		}

		void Execute () {
			if (path.length() == 0) {
				ret = -ENOENT;
				return;
			}
			ext2_ino_t ino = string_to_inode(fs, path.c_str());
			if (ino == 0) {
				ret = -ENOENT;
				return;
			}
			ret = ext2fs_check_directory(fs, ino);
			bool is_dir = (ret == 0);
			if (rmdir) {
				if (!is_dir) {
					ret = -ENOTDIR;
					return;
				}
			} else {
				if (is_dir) {
					ret = -EISDIR;
					return;
				}
			}
			// Remove the slash at the beginning if there is one.
			if (path.at(0) == '/') {
				path.erase(0, 1);
			}
			ret = ext2fs_unlink(fs, EXT2_ROOT_INO, path.c_str(), 0, 0);
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null()};
			callback->Call(1, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_filsys fs;
		std::string path;

	protected:
		bool rmdir;
};
X_NAN_METHOD(unlink_, UnlinkWorker, 3);

class RmDirWorker : public UnlinkWorker {
	public:
		RmDirWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: UnlinkWorker(info, callback) {
			rmdir = true;
		}
};
X_NAN_METHOD(rmdir, RmDirWorker, 3);

class MkDirWorker : public AsyncWorker {
	public:
		MkDirWorker(NAN_METHOD_ARGS_TYPE info, Callback *callback)
		: AsyncWorker(callback) {
			fs = get_filesystem(info);
			path = get_path(info);
			mode = static_cast<unsigned int>(Nan::To<int64_t>(info[2]).FromJust());
		}

		void Execute () {
			// TODO: free ?
			ext2_ino_t parent_ino = get_parent_dir_ino(fs, (char*)path.c_str());
			if (parent_ino == NULL) {
				ret = -ENOTDIR;
				return;
			}
			char* filename = get_filename(path.c_str());
			if (filename == NULL) {
				// This should never happen.
				ret = -EISDIR;
				return;
			}
			ext2_ino_t newdir;
			ret = ext2fs_new_inode(
				fs,
				parent_ino,
				LINUX_S_IFDIR,
				NULL,
				&newdir
			);
			if (ret) return;
			ret = ext2fs_mkdir(fs, parent_ino, newdir, filename);
			if (ret) return;
			struct ext2_inode inode;
			ret = ext2fs_read_inode(fs, newdir, &inode);
			if (ret) return;
			inode.i_mode = (mode & ~LINUX_S_IFMT) | LINUX_S_IFDIR;
			ret = ext2fs_write_inode(fs, newdir, &inode);
		}

		void HandleOKCallback () {
			if (ret) {
				v8::Local<v8::Value> argv[] = {ErrnoException(-ret)};
				callback->Call(1, argv, async_resource);
				return;
			}
			v8::Local<v8::Value> argv[] = {Null()};
			callback->Call(1, argv, async_resource);
		}

	private:
		errcode_t ret = 0;
		ext2_filsys fs;
		std::string path;
		unsigned int mode;
};
X_NAN_METHOD(mkdir, MkDirWorker, 4);

v8::Local<v8::Value> getUInt64Number(long unsigned int hi, long unsigned int lo) {
		long unsigned int size = lo | (hi << 32);
		return Nan::New<v8::Number>(static_cast<double>(size));
}

v8::Local<v8::Value> timespecToMilliseconds(__u32 seconds) {
		return Nan::New<v8::Number>(static_cast<double>(seconds) * 1000);
}

NAN_METHOD(fstat_) {
	// TODO handle 32 bit {u,g}ids
	CHECK_ARGS(4)
	auto file = get_file(info);
	auto flags = get_flags(info);
	auto Stats = info[2].As<v8::Function>();
	auto *callback = new Callback(info[3].As<v8::Function>());
	v8::Local<v8::Value> statsArgs[] = {
		Nan::New<v8::Number>(0),   // dev
		Nan::New<v8::Number>(file->inode.i_mode),
		Nan::New<v8::Number>(file->inode.i_links_count),
		Nan::New<v8::Number>(file->inode.i_uid),
		Nan::New<v8::Number>(file->inode.i_gid),
		Nan::New<v8::Number>(0),  // rdev
		Nan::New<v8::Number>(file->fs->blocksize),
		Nan::New<v8::Number>(file->ino),
		getUInt64Number(file->inode.i_size_high, file->inode.i_size),
		Nan::New<v8::Number>(file->inode.i_blocks),
		timespecToMilliseconds(file->inode.i_atime),
		timespecToMilliseconds(file->inode.i_mtime),
		timespecToMilliseconds(file->inode.i_ctime),
		timespecToMilliseconds(file->inode.i_ctime),
	};
	auto stats = Nan::NewInstance(Stats, 14, statsArgs).ToLocalChecked();
	v8::Local<v8::Value> argv[] = {Null(), stats};
	callback->Call(2, argv);
	delete callback;
}

NAN_METHOD(init) {
	init_async();
}

NAN_METHOD(closeExt) {
	close_async();
}
