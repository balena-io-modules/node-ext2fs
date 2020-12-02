#include <emscripten.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ext2fs.h"
#include "glue.h"

#define O_WRONLY    00000001
#define O_RDWR      00000002
#define O_CREAT     00000100
#define O_EXCL      00000200
#define O_TRUNC     00001000
#define O_APPEND    00002000
#define O_DIRECTORY 00200000
#define O_NOATIME   01000000

// Access js stuff from C
EM_JS(void, array_push_buffer, (int array_id, char* value, int len), {
	const heapBuffer = Module.getBuffer(value, len);
	const buffer = Buffer.alloc(len);
	heapBuffer.copy(buffer);
	Module.getObject(array_id).push(buffer);
});

EM_JS(errcode_t, blk_read, (int disk_id, short block_size, unsigned long block, unsigned long count, void *data), {
	return Asyncify.handleAsync(async () => {
		const offset = block * block_size;
		const size = count < 0 ? -count : count * block_size;
		const buffer = Module.getBuffer(data, size);
		const disk = Module.getObject(disk_id);
		try {
			await disk.read(buffer, 0, buffer.length, offset);
			return 0;
		} catch (error) {
			return Module.EIO;
		}
	});
});

EM_JS(errcode_t, blk_write, (int disk_id, short block_size, unsigned long block, unsigned long count, const void *data), {
	return Asyncify.handleAsync(async () => {
		const offset = block * block_size;
		const size = count < 0 ? -count : count * block_size;
		const buffer = Module.getBuffer(data, size);
		const disk = Module.getObject(disk_id);
		try {
			await disk.write(buffer, 0, buffer.length, offset);
			return 0;
		} catch (error) {
			return Module.EIO;
		}
	});
});

EM_JS(errcode_t, discard, (int disk_id, short block_size, unsigned long block, unsigned long count), {
	return Asyncify.handleAsync(async () => {
		const disk = Module.getObject(disk_id);
		const offset = block * block_size;
		const size = count < 0 ? -count : count * block_size;
		try {
			await disk.discard(offset, size);
			return 0;
		} catch (error) {
			return Module.EIO;
		}
	});
});

EM_JS(errcode_t, flush, (int disk_id), {
	return Asyncify.handleAsync(async () => {
		const disk = Module.getObject(disk_id);
		try {
			await disk.flush();
			return 0;
		} catch (error) {
			return Module.EIO;
		}
	});
});
// ------------------------

// Utils ------------------
ext2_ino_t string_to_inode(ext2_filsys fs, const char *str) {
	ext2_ino_t ino;
	int retval;
	retval = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, str, &ino);
	if (retval) {
		return 0;
	}
	return ino;
}

int copy_filename_to_result(
	struct ext2_dir_entry *dirent,
	int offset,
	int blocksize,
	char *buf,
	void *priv_data  // this is the js array_id
) {
	size_t len = ext2fs_dirent_name_len(dirent);
	if (
		(strncmp(dirent->name, ".", len) != 0) &&
		(strncmp(dirent->name, "..", len) != 0)
	) {
		array_push_buffer((int)priv_data, dirent->name, len);
	}
	return 0;
}

ext2_ino_t get_parent_dir_ino(ext2_filsys fs, const char* path) {
	char* last_slash = strrchr(path, '/');
	if (last_slash == NULL) {
		return 0;
	}
	unsigned int parent_len = last_slash - path + 1;
	char* parent_path = strndup(path, parent_len);
	ext2_ino_t parent_ino = string_to_inode(fs, parent_path);
	free(parent_path);
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
	// Returns a >= 0 error code
	errcode_t ret = 0;
	ext2_ino_t parent_ino = get_parent_dir_ino(fs, path);
	if (parent_ino == 0) {
		return ENOTDIR;
	}
	ret = ext2fs_new_inode(fs, parent_ino, mode, 0, ino);
	if (ret) return ret;
	char* filename = get_filename(path);
	if (filename == NULL) {
		// This should never happen.
		return EISDIR;
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
	inode.i_atime = inode.i_ctime = inode.i_mtime = time(0);
	inode.i_links_count = 1;
	ret = ext2fs_inode_size_set(fs, &inode, 0);  // TODO: update size? also on write?
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

errcode_t update_xtime(ext2_file_t file, bool a, bool c, bool m) {
	errcode_t err = 0;
	err = ext2fs_read_inode(file->fs, file->ino, &(file->inode));
	if (err) return err;
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	if (a) {
		file->inode.i_atime = now.tv_sec;
	}
	if (c) {
		file->inode.i_ctime = now.tv_sec;
	}
	if (m) {
		file->inode.i_mtime = now.tv_sec;
	}
	increment_version(&(file->inode));
	err = ext2fs_write_inode(file->fs, file->ino, &(file->inode));
	return err;
}

unsigned long getUInt64Number(unsigned long long hi, unsigned long long lo) {
	return lo | (hi << 32);
}
// ------------------------

// Call these from js -----

// We can read and write C memory from js but we can't read & write js Buffer data from C so we need a way to allocate & free C memory from js.
// That way we can allocate C memory from js, pass it to a C function to be written to and read it in js.
char* malloc_from_js(int length) {
	return malloc(length);
}

void free_from_js(char *data) {
	free(data);
}

errcode_t node_ext2fs_mount(int disk_id) {
	ext2_filsys fs;
	char hex_ptr[sizeof(void*) * 2 + 3];
	sprintf(hex_ptr, "%d", disk_id);
	errcode_t ret = ext2fs_open(
		hex_ptr,              // name
		EXT2_FLAG_RW,         // flags
		0,                    // superblock
		0,                    // block_size
		get_js_io_manager(),  // manager
		&fs                   // ret_fs
	);
	if (ret) { 
		return -ret;
	}
	ret = ext2fs_read_bitmaps(fs);
	if (ret) {
		return -ret;
	}
	return (long)fs;
}

errcode_t node_ext2fs_trim(ext2_filsys fs) {
	unsigned int start, blk, count;
	errcode_t ret;
	if (!fs->block_map) {
		if ((ret = ext2fs_read_block_bitmap(fs))) {
			return -ret;
		}
	}
	start = fs->super->s_first_data_block;
	count = fs->super->s_blocks_count;
	for (blk = start; blk <= count; blk++) {
		// Check for either the last iteration or a used block
		if (blk == count || ext2fs_test_block_bitmap(fs->block_map, blk)) {
			if (start < blk) {
				if ((ret = io_channel_discard(fs->io, start, blk - start))) {
					return -ret;
				}
			}
			start = blk + 1;
		}
	}
	return 0;
}

errcode_t node_ext2fs_readdir(ext2_filsys fs, char* path, int array_id) {
	ext2_ino_t ino = string_to_inode(fs, path);
	if (ino == 0) {
		return -ENOENT;
	}
	ext2_file_t file;
	errcode_t ret = ext2fs_file_open(
		fs,
		ino, // inode,
		0, // flags TODO
		&file
	);
	if (ret) return -ret;
	ret = ext2fs_check_directory(fs, ino);
	if (ret) return -ret;
	char* block_buf = malloc(fs->blocksize);
	ret = ext2fs_dir_iterate(
		fs,
		ino,
		0,  // flags
		block_buf,
		copy_filename_to_result,
		(void*)array_id
	);
	free(block_buf);
	return -ret;
}

long node_ext2fs_open(ext2_filsys fs, char* path, unsigned int flags, unsigned int mode) {
	// TODO: O_NOFOLLOW, O_SYMLINK
	ext2_ino_t ino = string_to_inode(fs, path);
	errcode_t ret;
	if (ino == 0) {
		if (!(flags & O_CREAT)) {
			return -ENOENT;
		}
		ret = create_file(fs, path, mode, &ino);
		if (ret) return -ret;
	} else if (flags & O_EXCL) {
		return -EEXIST;
	}
	if ((flags & O_DIRECTORY) && ext2fs_check_directory(fs, ino)) {
		return -ENOTDIR;
	}
	ext2_file_t file;
	ret = ext2fs_file_open(fs, ino, translate_open_flags(flags), &file);
	if (ret) return -ret;
	if (flags & O_TRUNC) {
		ret = ext2fs_file_set_size2(file, 0);
		if (ret) return -ret;
	}
	return (long)file;
}

long node_ext2fs_read(
	ext2_file_t file,
	int flags,
	char *buffer,
	unsigned long length,  // requested length
	unsigned long position  // position in file, -1 for current position
) {
	errcode_t ret = 0;
	if ((flags & O_WRONLY) != 0) {
		// Don't try to read write only files.
		return -EBADF;
	}
	if (position != -1) {
		ret = ext2fs_file_llseek(file, position, EXT2_SEEK_SET, NULL);
		if (ret) return -ret;
	}
	unsigned int got;
	ret = ext2fs_file_read(file, buffer, length, &got);
	if (ret) return -ret;
	if ((flags & O_NOATIME) == 0) {
		ret = update_xtime(file, true, false, false);
		if (ret) return -ret;
	}
	return got;
}

long node_ext2fs_write(
	ext2_file_t file,
	int flags,
	char *buffer,
	unsigned long length,  // requested length
	unsigned long position  // position in file, -1 for current position
) {
	if ((flags & (O_WRONLY | O_RDWR)) == 0) {
		// Don't try to write to readonly files.
		return -EBADF;
	}
	errcode_t ret = 0;
	if ((flags & O_APPEND) != 0) {
		// append mode: seek to the end before each write
		ret = ext2fs_file_llseek(file, 0, EXT2_SEEK_END, NULL);
	} else if (position != -1) {
		ret = ext2fs_file_llseek(file, position, EXT2_SEEK_SET, NULL);
	}
	if (ret) return -ret;
	unsigned int written;
	ret = ext2fs_file_write(file, buffer, length, &written);
	if (ret) return -ret;
	if ((flags & O_CREAT) != 0) {
		ret = update_xtime(file, false, true, true);
		if (ret) return -ret;
	}
	return written;
}

errcode_t node_ext2fs_mkdir(
	ext2_filsys fs,
	const char *path,
	int mode
) {
	ext2_ino_t parent_ino = get_parent_dir_ino(fs, path);
	if (parent_ino == 0) {
		return -ENOTDIR;
	}
	char* filename = get_filename(path);
	if (filename == NULL) {
		// This should never happen.
		return -EISDIR;
	}
	ext2_ino_t newdir;
	errcode_t ret;
	ret = ext2fs_new_inode(
		fs,
		parent_ino,
		LINUX_S_IFDIR,
		NULL,
		&newdir
	);
	if (ret) return -ret;
	ret = ext2fs_mkdir(fs, parent_ino, newdir, filename);
	if (ret) return -ret;
	struct ext2_inode inode;
	ret = ext2fs_read_inode(fs, newdir, &inode);
	if (ret) return -ret;
	inode.i_mode = (mode & ~LINUX_S_IFMT) | LINUX_S_IFDIR;
	ret = ext2fs_write_inode(fs, newdir, &inode);
	return -ret;
}

errcode_t node_ext2fs_unlink(
	ext2_filsys fs,
	const char *path,
	bool rmdir
) {
	if (strlen(path) == 0) {
		return -ENOENT;
	}
	ext2_ino_t ino = string_to_inode(fs, path);
	if (ino == 0) {
		return -ENOENT;
	}
	errcode_t ret;
	ret = ext2fs_check_directory(fs, ino);
	bool is_dir = (ret == 0);
	if (rmdir) {
		if (!is_dir) {
			return -ENOTDIR;
		}
	} else {
		if (is_dir) {
			return -EISDIR;
		}
	}
	ext2_ino_t parent_ino = get_parent_dir_ino(fs, path);
	if (parent_ino == 0) {
		return -ENOENT;
	}
	ret = ext2fs_unlink(fs, parent_ino, NULL, ino, 0);
	return -ret;
}

errcode_t node_ext2fs_chmod(
	ext2_file_t file,
	int mode
) {
	errcode_t ret = ext2fs_read_inode(file->fs, file->ino, &(file->inode));
	if (ret) return -ret;
	// keep only fmt (file or directory)
	file->inode.i_mode &= LINUX_S_IFMT;
	// apply new mode
	file->inode.i_mode |= (mode & ~LINUX_S_IFMT);
	increment_version(&(file->inode));
	ret = ext2fs_write_inode(file->fs, file->ino, &(file->inode));
	return -ret;
}

errcode_t node_ext2fs_chown(
	ext2_file_t file,
	int uid,
	int gid
) {
	// TODO handle 32 bit {u,g}ids
	errcode_t ret = ext2fs_read_inode(file->fs, file->ino, &(file->inode));
	if (ret) return -ret;
	// keep only the lower 16 bits
	file->inode.i_uid = uid & 0xFFFF;
	file->inode.i_gid = gid & 0xFFFF;
	increment_version(&(file->inode));
	ret = ext2fs_write_inode(file->fs, file->ino, &(file->inode));
	return -ret;
}

errcode_t node_ext2fs_close(ext2_file_t file) {
	return -ext2fs_file_close(file);
}

int node_ext2fs_stat_i_mode(ext2_file_t file) {
	return file->inode.i_mode;
}

int node_ext2fs_stat_i_links_count(ext2_file_t file) {
	return file->inode.i_links_count;
}

int node_ext2fs_stat_i_uid(ext2_file_t file) {
	return file->inode.i_uid;
}

int node_ext2fs_stat_i_gid(ext2_file_t file) {
	return file->inode.i_gid;
}

int node_ext2fs_stat_blocksize(ext2_file_t file) {
	return file->fs->blocksize;
}

unsigned long node_ext2fs_stat_i_size(ext2_file_t file) {
	return getUInt64Number(file->inode.i_size_high, file->inode.i_size);
}

int node_ext2fs_stat_ino(ext2_file_t file) {
	return file->ino;
}

int node_ext2fs_stat_i_blocks(ext2_file_t file) {
	return file->inode.i_blocks;
}

int node_ext2fs_stat_i_atime(ext2_file_t file) {
	return file->inode.i_atime;
}

int node_ext2fs_stat_i_mtime(ext2_file_t file) {
	return file->inode.i_mtime;
}

int node_ext2fs_stat_i_ctime(ext2_file_t file) {
	return file->inode.i_ctime;
}

errcode_t node_ext2fs_umount(ext2_filsys fs) {
	return -ext2fs_close(fs);
}
//-------------------------------------------
int get_disk_id(io_channel channel) {
	return (int)channel->private_data;
}

static errcode_t js_open_entry(const char *disk_id_str, int flags, io_channel *channel) {
	io_channel io = NULL;
	errcode_t ret = ext2fs_get_mem(sizeof(struct struct_io_channel), &io);
	if (ret) {
		return ret;
	}
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	io->manager = get_js_io_manager();
	sscanf(disk_id_str, "%d", (int*)&io->private_data);
	*channel = io;
	return 0;
}

static errcode_t js_close_entry(io_channel channel) {
	return ext2fs_free_mem(&channel);
}

static errcode_t set_blksize(io_channel channel, int blksize) {
	channel->block_size = blksize;
	return 0;
}

static errcode_t js_read_blk_entry(io_channel channel, unsigned long block, int count, void *data) {
	int disk_id = get_disk_id(channel);
	return blk_read(disk_id, channel->block_size, block, count, data);
}

static errcode_t js_write_blk_entry(io_channel channel, unsigned long block, int count, const void *data) {
	int disk_id = get_disk_id(channel);
	return blk_write(disk_id, channel->block_size, block, count, data);
}

static errcode_t js_flush_entry(io_channel channel) {
	int disk_id = get_disk_id(channel);
	return flush(disk_id);
}

static errcode_t js_read_blk64_entry(io_channel channel, unsigned long long block, int count, void *data) {
	return js_read_blk_entry(channel, block, count, data);
}

static errcode_t js_write_blk64_entry(io_channel channel, unsigned long long block, int count, const void *data) {
	return js_write_blk_entry(channel, block, count, data);
}

static errcode_t js_discard_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	int disk_id = get_disk_id(channel);
	return discard(disk_id, channel->block_size, block, count);
}

static errcode_t js_cache_readahead_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	return 0;
}

static errcode_t js_zeroout_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	int disk_id = get_disk_id(channel);
	unsigned long long size = (count < 0) ? -count : count * channel->block_size;
	char *data = malloc(size);
	memset(data, 0, size);
	errcode_t ret = blk_write(disk_id, channel->block_size, block, count, data);
	if (ret) return ret;
	return discard(disk_id, channel->block_size, block, count);
}

struct struct_io_manager js_io_manager;

io_manager get_js_io_manager() {
	js_io_manager.magic            =  EXT2_ET_MAGIC_IO_MANAGER;
	js_io_manager.name             =  "JavaScript IO Manager";
	js_io_manager.open             =  js_open_entry;
	js_io_manager.close            =  js_close_entry;
	js_io_manager.set_blksize      =  set_blksize;
	js_io_manager.read_blk         =  js_read_blk_entry;
	js_io_manager.write_blk        =  js_write_blk_entry;
	js_io_manager.flush            =  js_flush_entry;
	js_io_manager.read_blk64       =  js_read_blk64_entry;
	js_io_manager.write_blk64      =  js_write_blk64_entry;
	js_io_manager.discard          =  js_discard_entry;
	js_io_manager.cache_readahead  =  js_cache_readahead_entry;
	js_io_manager.zeroout          =  js_zeroout_entry;
	return &js_io_manager;
};
