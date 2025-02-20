#include <emscripten.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ext2fs.h"
#include "glue.h"

#define O_WRONLY		00000001
#define O_RDWR			00000002
#define O_CREAT		 00000100
#define O_EXCL			00000200
#define O_TRUNC		 00001000
#define O_APPEND		00002000
#define O_DIRECTORY 00200000
#define O_NOFOLLOW	 00400000
#define O_NOATIME	 01000000

/*
 * Extended fields will fit into an inode if the filesystem was formatted
 * with large inodes (-I 256 or larger) and there are not currently any EAs
 * consuming all of the available space. For new inodes we always reserve
 * enough space for the kernel's known extended fields, but for inodes
 * created with an old kernel this might not have been the case. None of
 * the extended inode fields is critical for correct filesystem operation.
 * This macro checks if a certain field fits in the inode. Note that
 * inode-size = GOOD_OLD_INODE_SIZE + i_extra_isize
 */
#define EXT4_FITS_IN_INODE(ext4_inode, field)		\
	((offsetof(typeof(*ext4_inode), field) +	\
		sizeof((ext4_inode)->field))			\
	 <= ((size_t) EXT2_GOOD_OLD_INODE_SIZE +		\
			(ext4_inode)->i_extra_isize))		\

static inline __u32 ext4_encode_extra_time(const struct timespec *time) {
	__u32 extra = sizeof(time->tv_sec) > 4 ?
			((time->tv_sec - (__s32)time->tv_sec) >> 32) &
			EXT4_EPOCH_MASK : 0;
	return extra | (time->tv_nsec << EXT4_EPOCH_BITS);
}

static inline void ext4_decode_extra_time(struct timespec *time, __u32 extra) {
	if (sizeof(time->tv_sec) > 4 && (extra & EXT4_EPOCH_MASK)) {
		__u64 extra_bits = extra & EXT4_EPOCH_MASK;
		/*
		 * Prior to kernel 3.14?, we had a broken decode function,
		 * wherein we effectively did this:
		 * if (extra_bits == 3)
		 *		 extra_bits = 0;
		 */
		time->tv_sec += extra_bits << 32;
	}
	time->tv_nsec = ((extra) & EXT4_NSEC_MASK) >> EXT4_EPOCH_BITS;
}

#define EXT4_INODE_SET_XTIME(xtime, timespec, raw_inode) \
do {																										 \
	(raw_inode)->xtime = (timespec)->tv_sec;							 \
	if (EXT4_FITS_IN_INODE(raw_inode, xtime ## _extra))		\
		(raw_inode)->xtime ## _extra =											 \
				ext4_encode_extra_time(timespec);								\
} while (0)

#define EXT4_INODE_GET_XTIME(xtime, timespec, raw_inode)					 \
do {												 \
	(timespec)->tv_sec = (signed)((raw_inode)->xtime);					 \
	if (EXT4_FITS_IN_INODE(raw_inode, xtime ## _extra))					 \
		ext4_decode_extra_time((timespec),						 \
							 (raw_inode)->xtime ## _extra);				 \
	else											 \
		(timespec)->tv_nsec = 0;							 \
} while (0)




static int __translate_error(ext2_filsys fs, errcode_t err, ext2_ino_t ino,
					 const char *file, int line);
#define translate_error(fs, ino, err) __translate_error((fs), (err), (ino), \
			__FILE__, __LINE__)

#ifdef DEBUG
# define dbg_pf(f, a...)	do {printf("node_ext2fs-" f, ## a); \
	fflush(stdout); \
} while (0)
#else
# define dbg_pf(f, a...)
#endif


// Access js stuff from C
EM_JS(void, array_push_buffer, (int array_id, char* value, int len), {
	const heapBuffer = Module.getBuffer(value, len);
	const buffer = Buffer.allocUnsafe(len);
	heapBuffer.copy(buffer);
	Module.getObject(array_id).push(buffer);
});

EM_ASYNC_JS(errcode_t, blk_read, (int disk_id, short block_size, unsigned long block, unsigned long count, void *data), {
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

EM_ASYNC_JS(errcode_t, blk_write, (int disk_id, short block_size, unsigned long block, unsigned long count, const void *data), {
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

EM_ASYNC_JS(errcode_t, discard, (int disk_id, short block_size, unsigned long block, unsigned long count), {
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

EM_ASYNC_JS(errcode_t, flush, (int disk_id), {
	const disk = Module.getObject(disk_id);
	try {
		await disk.flush();
		return 0;
	} catch (error) {
		return Module.EIO;
	}
});

// Utils ------------------
ext2_ino_t string_to_inode(ext2_filsys fs, const char *str, int follow) {
	ext2_ino_t ino;
	int retval;
	if (follow) {
		retval = ext2fs_namei_follow(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, str, &ino);
	} else {
		retval = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, str, &ino);
	}
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
	void *priv_data	// this is the js array_id
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
	if (last_slash == 0) {
		return 0;
	}
	unsigned int parent_len = last_slash - path + 1;
	char* parent_path = strndup(path, parent_len);
	ext2_ino_t parent_ino = string_to_inode(fs, parent_path, 1);
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
	ret = ext2fs_inode_size_set(fs, &inode, 0);	// TODO: update size? also on write?
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
static void get_now(struct timespec *now) {
#ifdef CLOCK_REALTIME
	if (!clock_gettime(CLOCK_REALTIME, now))
		return;
#endif

	now->tv_sec = time(NULL);
	now->tv_nsec = 0;
}

errcode_t update_xtime(ext2_file_t file, bool a, bool c, bool m) {
	errcode_t err = 0;
	err = ext2fs_read_inode(file->fs, file->ino, &(file->inode));
	if (err) return err;
	struct timespec now;
	get_now(&now);
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

double getUInt64Number(unsigned long long hi, unsigned long long lo) {
	return (double) ((hi << 32) | lo);
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
		hex_ptr,							// name
		EXT2_FLAG_RW,				 // flags
		0,										// superblock
		0,										// block_size
		get_js_io_manager(),	// manager
		&fs									 // ret_fs
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
	ext2_ino_t ino = string_to_inode(fs, path, 1);
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
		0,	// flags
		block_buf,
		copy_filename_to_result,
		(void*)array_id
	);
	free(block_buf);
	return -ret;
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

long node_ext2fs_open(ext2_filsys fs, char* path, unsigned int flags, unsigned int mode) {
	ext2_ino_t ino = string_to_inode(fs, path, !(flags & O_NOFOLLOW));
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
	unsigned long length,	// requested length
	unsigned long position	// position in file, -1 for current position
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
	unsigned long length,	// requested length
	unsigned long position	// position in file, -1 for current position
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

	ret = ext2fs_file_flush(file);
	if (ret) {
		ret = translate_error(file->fs, file->ino, ret);
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

struct update_dotdot {
	ext2_ino_t new_dotdot;
};

static int update_dotdot_helper(
	ext2_ino_t dir,
	int entry,
	struct ext2_dir_entry *dirent,
	int offset,
	int blocksize,
	char *buf,
	void *priv_data
) {
	struct update_dotdot *ud = priv_data;

	if (ext2fs_dirent_name_len(dirent) == 2 &&
			dirent->name[0] == '.' && dirent->name[1] == '.') {
		dirent->inode = ud->new_dotdot;
		return DIRENT_CHANGED | DIRENT_ABORT;
	}

	return 0;
}

errcode_t node_ext2fs_rename(ext2_filsys fs, const char *from, const char *to) {
	errcode_t err;
	ext2_ino_t from_ino, to_ino, to_dir_ino, from_dir_ino;
	char *temp_to = NULL, *temp_from = NULL;
	char *cp, a;
	struct ext2_inode inode;
	struct update_dotdot ud;
	int ret = 0;

	dbg_pf("%s: renaming %s to %s\n", __func__, from, to);

	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, from, &from_ino);
	if (err || from_ino == 0) {
		ret = translate_error(fs, 0, err);
		goto out;
	}

	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, to, &to_ino);
	if (err && err != EXT2_ET_FILE_NOT_FOUND) {
		ret = translate_error(fs, 0, err);
		goto out;
	}

	if (err == EXT2_ET_FILE_NOT_FOUND)
		to_ino = 0;

	/* Already the same file? */
	if (to_ino != 0 && to_ino == from_ino) {
		ret = 0;
		goto out;
	}

	temp_to = strdup(to);
	if (!temp_to) {
		ret = -ENOMEM;
		goto out;
	}

	temp_from = strdup(from);
	if (!temp_from) {
		ret = -ENOMEM;
		goto out2;
	}

	/* Find parent dir of the source and check write access */
	cp = strrchr(temp_from, '/');
	if (!cp) {
		ret = -EINVAL;
		goto out2;
	}

	a = *(cp + 1);
	*(cp + 1) = 0;
	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, temp_from,
				 &from_dir_ino);
	*(cp + 1) = a;
	if (err) {
		ret = translate_error(fs, 0, err);
		goto out2;
	}
	if (from_dir_ino == 0) {
		ret = -ENOENT;
		goto out2;
	}

	/* Find parent dir of the destination and check write access */
	cp = strrchr(temp_to, '/');
	if (!cp) {
		ret = -EINVAL;
		goto out2;
	}

	a = *(cp + 1);
	*(cp + 1) = 0;
	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, temp_to,
				 &to_dir_ino);
	*(cp + 1) = a;
	if (err) {
		ret = translate_error(fs, 0, err);
		goto out2;
	}
	if (to_dir_ino == 0) {
		ret = -ENOENT;
		goto out2;
	}

	/* If the target exists, unlink it first */
	if (to_ino != 0) {
		err = ext2fs_read_inode(fs, to_ino, &inode);
		if (err) {
			ret = translate_error(fs, to_ino, err);
			goto out2;
		}

		dbg_pf("%s: unlinking %s ino=%d\n", __func__,
				 LINUX_S_ISDIR(inode.i_mode) ? "dir" : "file",
				 to_ino);
		if (LINUX_S_ISDIR(inode.i_mode))
			ret = node_ext2fs_rmdir(fs, to);
		else
			ret = node_ext2fs_unlink(fs, to);
		if (ret)
			goto out2;
	}

	/* Get ready to do the move */
	err = ext2fs_read_inode(fs, from_ino, &inode);
	if (err) {
		ret = translate_error(fs, from_ino, err);
		goto out2;
	}

	/* Link in the new file */
	dbg_pf("%s: linking ino=%d/path=%s to dir=%d\n", __func__,
			 from_ino, cp + 1, to_dir_ino);
	err = ext2fs_link(fs, to_dir_ino, cp + 1, from_ino,
				ext2_file_type(inode.i_mode));
	if (err == EXT2_ET_DIR_NO_SPACE) {
		err = ext2fs_expand_dir(fs, to_dir_ino);
		if (err) {
			ret = translate_error(fs, to_dir_ino, err);
			goto out2;
		}

		err = ext2fs_link(fs, to_dir_ino, cp + 1, from_ino,
						 ext2_file_type(inode.i_mode));
	}
	if (err) {
		ret = translate_error(fs, to_dir_ino, err);
		goto out2;
	}

	/* Update '..' pointer if dir */
	err = ext2fs_read_inode(fs, from_ino, &inode);
	if (err) {
		ret = translate_error(fs, from_ino, err);
		goto out2;
	}

	if (LINUX_S_ISDIR(inode.i_mode)) {
		ud.new_dotdot = to_dir_ino;
		dbg_pf("%s: updating .. entry for dir=%d\n", __func__,
				 to_dir_ino);
		err = ext2fs_dir_iterate2(fs, from_ino, 0, NULL,
						update_dotdot_helper, &ud);
		if (err) {
			ret = translate_error(fs, from_ino, err);
			goto out2;
		}

		/* Decrease from_dir_ino's links_count */
		dbg_pf("%s: moving linkcount from dir=%d to dir=%d\n",
				 __func__, from_dir_ino, to_dir_ino);
		err = ext2fs_read_inode(fs, from_dir_ino, &inode);
		if (err) {
			ret = translate_error(fs, from_dir_ino, err);
			goto out2;
		}
		inode.i_links_count--;
		err = ext2fs_write_inode(fs, from_dir_ino, &inode);
		if (err) {
			ret = translate_error(fs, from_dir_ino, err);
			goto out2;
		}

		/* Increase to_dir_ino's links_count */
		err = ext2fs_read_inode(fs, to_dir_ino, &inode);
		if (err) {
			ret = translate_error(fs, to_dir_ino, err);
			goto out2;
		}
		inode.i_links_count++;
		err = ext2fs_write_inode(fs, to_dir_ino, &inode);
		if (err) {
			ret = translate_error(fs, to_dir_ino, err);
			goto out2;
		}
	}

	/* Update timestamps */
	ret = update_ctime(fs, from_ino, NULL);
	if (ret)
		goto out2;

	ret = update_mtime(fs, to_dir_ino, NULL);
	if (ret)
		goto out2;

	/* Remove the old file */
	ret = unlink_file_by_name(fs, from);
	if (ret)
		goto out2;

	/* Flush the whole mess out */
	err = ext2fs_flush2(fs, 0);
	if (err)
		ret = translate_error(fs, 0, err);

out2:
	free(temp_from);
	free(temp_to);
out:
	return ret;
}

errcode_t node_ext2fs_link(ext2_filsys fs, const char *src, const char *dest)
{
	char *temp_path;
	errcode_t err;
	char *node_name, a;
	ext2_ino_t parent, ino;
	struct ext2_inode_large inode;
	int ret = 0;

	dbg_pf("%s: src=%s dest=%s\n", __func__, src, dest);
	temp_path = strdup(dest);
	if (!temp_path) {
		ret = -ENOMEM;
		goto out;
	}
	node_name = strrchr(temp_path, '/');
	if (!node_name) {
		ret = -ENOMEM;
		goto out;
	}
	node_name++;
	a = *node_name;
	*node_name = 0;

	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, temp_path, &parent);
	*node_name = a;
	if (err) {
		err = -ENOENT;
		goto out;
	}


	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, src, &ino);
	if (err || ino == 0) {
		ret = translate_error(fs, 0, err);
		goto out;
	}

	memset(&inode, 0, sizeof(inode));
	err = ext2fs_read_inode_full(fs, ino, (struct ext2_inode *)&inode, sizeof(inode));
	if (err) {
		ret = translate_error(fs, ino, err);
		goto out;
	}

	inode.i_links_count++;
	ret = update_ctime(fs, ino, &inode);
	if (ret)
		goto out;

	err = ext2fs_write_inode_full(fs, ino, (struct ext2_inode *)&inode, sizeof(inode));
	if (err) {
		ret = translate_error(fs, ino, err);
		goto out;
	}

	dbg_pf("%s: linking ino=%d/name=%s to dir=%d\n", __func__, ino, node_name, parent);
	err = ext2fs_link(fs, parent, node_name, ino, ext2_file_type(inode.i_mode));
	if (err == EXT2_ET_DIR_NO_SPACE) {
		err = ext2fs_expand_dir(fs, parent);
		if (err) {
			ret = translate_error(fs, parent, err);
			goto out;
		}

		err = ext2fs_link(fs, parent, node_name, ino, ext2_file_type(inode.i_mode));
	}
	if (err) {
		ret = translate_error(fs, parent, err);
		goto out;
	}

	ret = update_mtime(fs, parent, NULL);
	if (ret)
		goto out;

out:
	free(temp_path);
	return ret;
}

int node_ext2fs_symlink(ext2_filsys fs, const char *src, const char *dest) {
	ext2_ino_t parent, child;
	char *temp_path;
	errcode_t err;
	char *node_name, a;
	struct ext2_inode_large inode;
	int ret = 0;

	dbg_pf("%s: symlink %s to %s\n", __func__, src, dest);
	temp_path = strdup(dest);
	if (!temp_path) {
		ret = -ENOMEM;
		goto out;
	}
	node_name = strrchr(temp_path, '/');
	if (!node_name) {
		ret = -ENOMEM;
		goto out;
	}
	node_name++;
	a = *node_name;
	*node_name = 0;

	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, temp_path,
				 &parent);
	*node_name = a;
	if (err) {
		ret = translate_error(fs, 0, err);
		goto out;
	}

	/* Create symlink */
	err = ext2fs_symlink(fs, parent, 0, node_name, src);
	if (err == EXT2_ET_DIR_NO_SPACE) {
		err = ext2fs_expand_dir(fs, parent);
		if (err) {
			ret = translate_error(fs, parent, err);
			goto out;
		}

		err = ext2fs_symlink(fs, parent, 0, node_name, src);
	}
	if (err) {
		ret = translate_error(fs, parent, err);
		goto out;
	}

	/* Update parent dir's mtime */
	ret = update_mtime(fs, parent, NULL);
	if (ret)
		goto out;

	/* Still have to update the uid/gid of the symlink */
	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, temp_path,
				 &child);
	if (err) {
		ret = translate_error(fs, 0, err);
		goto out;
	}
	dbg_pf("%s: symlinking ino=%d/name=%s to dir=%d\n", __func__,
			 child, node_name, parent);

	memset(&inode, 0, sizeof(inode));
	err = ext2fs_read_inode_full(fs, child, (struct ext2_inode *)&inode,
						 sizeof(inode));
	if (err) {
		ret = translate_error(fs, child, err);
		goto out;
	}

	err = ext2fs_write_inode_full(fs, child, (struct ext2_inode *)&inode,
							sizeof(inode));
	if (err) {
		ret = translate_error(fs, child, err);
		goto out;
	}
out:
	free(temp_path);
	return ret;
}

static int remove_inode(ext2_filsys fs, ext2_ino_t ino)
{
	errcode_t err;
	struct ext2_inode_large inode;
	int ret = 0;

	memset(&inode, 0, sizeof(inode));
	err = ext2fs_read_inode_full(fs, ino, (struct ext2_inode *)&inode,
						 sizeof(inode));
	if (err) {
		ret = translate_error(fs, ino, err);
		goto out;
	}
	dbg_pf("%s: put ino=%d links=%d\n", __func__, ino,
			 inode.i_links_count);

	switch (inode.i_links_count) {
	case 0:
		return 0; /* XXX: already done? */
	case 1:
		inode.i_links_count--;
		inode.i_dtime = time(0);
		break;
	default:
		inode.i_links_count--;
	}

	ret = update_ctime(fs, ino, &inode);
	if (ret)
		goto out;

	if (inode.i_links_count)
		goto write_out;

	/* Nobody holds this file; free its blocks! */
	err = ext2fs_free_ext_attr(fs, ino, &inode);
	if (err)
		goto write_out;

	if (ext2fs_inode_has_valid_blocks2(fs, (struct ext2_inode *)&inode)) {
		err = ext2fs_punch(fs, ino, (struct ext2_inode *)&inode, NULL,
					 0, ~0ULL);
		if (err) {
			ret = translate_error(fs, ino, err);
			goto write_out;
		}
	}

	ext2fs_inode_alloc_stats2(fs, ino, -1,
					LINUX_S_ISDIR(inode.i_mode));

write_out:
	err = ext2fs_write_inode_full(fs, ino, (struct ext2_inode *)&inode,
							sizeof(inode));
	if (err) {
		ret = translate_error(fs, ino, err);
		goto out;
	}
out:
	return ret;
}

errcode_t node_ext2fs_unlink(ext2_filsys fs, const char *path) {
	ext2_ino_t ino;
	errcode_t err;
	int ret = 0;

	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, path, &ino);
	if (err) {
		ret = translate_error(fs, 0, err);
		goto out;
	}

	if (ext2fs_check_directory(fs, ino) == 0) {
		return -EISDIR;
	};

	ret = unlink_file_by_name(fs, path);
	if (ret)
		goto out;

	ret = remove_inode(fs, ino);

out:
	return ret;
}

static int unlink_file_by_name(ext2_filsys fs, const char *path) {
	errcode_t err;
	ext2_ino_t dir;
	char *filename = strdup(path);
	char *base_name;
	int ret;

	base_name = strrchr(filename, '/');
	if (base_name) {
		*base_name++ = '\0';
		err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, filename,
					 &dir);
		if (err) {
			free(filename);
			return translate_error(fs, 0, err);
		}
	} else {
		dir = EXT2_ROOT_INO;
		base_name = filename;
	}

	dbg_pf("%s: unlinking name=%s from dir=%d\n", __func__,
			 base_name, dir);
	err = ext2fs_unlink(fs, dir, base_name, 0, 0);
	free(filename);
	if (err)
		return translate_error(fs, dir, err);

	return update_mtime(fs, dir, NULL);
}

struct rd_struct {
	ext2_ino_t	parent;
	int		empty;
};

errcode_t node_ext2fs_rmdir(ext2_filsys fs, const char *path) {
	ext2_ino_t child;
	errcode_t err;
	struct ext2_inode_large inode;
	struct rd_struct rds;
	int ret = 0;

	err = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO, path, &child);
	if (err) {
		ret = translate_error(fs, 0, err);
		goto out;
	}

	dbg_pf("%s: rmdir path=%s ino=%d\n", __func__, path, child);

	rds.parent = 0;
	rds.empty = 1;

	err = ext2fs_dir_iterate2(fs, child, 0, 0, rmdir_proc, &rds);
	if (err) {
		ret = translate_error(fs, child, err);
		goto out;
	}

	if (rds.empty == 0) {
		ret = -ENOTEMPTY;
		goto out;
	}

	ret = unlink_file_by_name(fs, path);
	if (ret)
		goto out;
	/* Directories have to be "removed" twice. */
	ret = remove_inode(fs, child);
	if (ret)
		goto out;
	ret = remove_inode(fs, child);
	if (ret)
		goto out;

	if (rds.parent) {
		dbg_pf("%s: decr dir=%d link count\n", __func__,
				 rds.parent);
		err = ext2fs_read_inode_full(fs, rds.parent,
							 (struct ext2_inode *)&inode,
							 sizeof(inode));
		if (err) {
			ret = translate_error(fs, rds.parent, err);
			goto out;
		}
		if (inode.i_links_count > 1)
			inode.i_links_count--;
		ret = update_mtime(fs, rds.parent, &inode);
		if (ret)
			goto out;
		err = ext2fs_write_inode_full(fs, rds.parent,
								(struct ext2_inode *)&inode,
								sizeof(inode));
		if (err) {
			ret = translate_error(fs, rds.parent, err);
			goto out;
		}
	}

out:
	return ret;
}

static int rmdir_proc(
	ext2_ino_t dir,
	int	entry,
	struct ext2_dir_entry *dirent,
	int	offset,
	int	blocksize,
	char	*buf,
	void	*private
) {
	struct rd_struct *rds = (struct rd_struct *) private;

	if (dirent->inode == 0)
		return 0;
	if (((dirent->name_len & 0xFF) == 1) && (dirent->name[0] == '.'))
		return 0;
	if (((dirent->name_len & 0xFF) == 2) && (dirent->name[0] == '.') &&
			(dirent->name[1] == '.')) {
		rds->parent = dirent->inode;
		return 0;
	}
	rds->empty = 0;
	return 0;
}


errcode_t node_ext2fs_chmod(ext2_file_t file, int mode) {
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

errcode_t node_ext2fs_readlink(
	ext2_filsys fs,
	const char* path,
	int array_id
) {
	errcode_t ret = 0;
	struct ext2_inode ei;
	char* buffer = 0;
	char* pathname;
	blk64_t blk;

	ext2_ino_t ino = string_to_inode(fs, path, 0);
	if (!ino) {
		return -ENOENT;
	}

	ret = ext2fs_read_inode(fs, ino, &ei);
	if (ret) {
		return -ret;
	}

	if (!LINUX_S_ISLNK(ei.i_mode)) {
		return -EINVAL;
	}

	if (ext2fs_is_fast_symlink(&ei)) {
		pathname = (char *)&(ei.i_block[0]);
	}
	else if (ei.i_flags & EXT4_INLINE_DATA_FL) {
		ret = ext2fs_get_memzero(ei.i_size, &buffer);
		if (ret) {
			return -ret;
		}

		ret = ext2fs_inline_data_get(fs, ino, &ei, buffer, NULL);
		if (ret) {
			ext2fs_free_mem(&buffer);
			return -ret;
		}

		pathname = buffer;
	} else {
		ret = ext2fs_bmap2(fs, ino, &ei, NULL, 0, 0, NULL, &blk);
		if (ret) {
			return -ret;
		}

		ret = ext2fs_get_mem(fs->blocksize, &buffer);
		if (ret) {
			return -ret;
		}

		ret = io_channel_read_blk64(fs->io, blk, 1, buffer);
		if (ret) {
			ext2fs_free_mem(&buffer);
			return -ret;
		}

		pathname = buffer;
	}

	array_push_buffer(array_id, pathname, strlen(pathname));

	if (buffer) {
		ext2fs_free_mem(&buffer);
	}

	return ret;
}

errcode_t node_ext2fs_close(ext2_file_t file) {
	return -ext2fs_file_close(file);
}

static int update_ctime(ext2_filsys fs, ext2_ino_t ino,
			struct ext2_inode_large *pinode) {
	errcode_t err;
	struct timespec now;
	struct ext2_inode_large inode;

	get_now(&now);

	/* If user already has a inode buffer, just update that */
	if (pinode) {
	increment_version((struct ext2_inode *) &inode);
		EXT4_INODE_SET_XTIME(i_ctime, &now, pinode);
		return 0;
	}

	/* Otherwise we have to read-modify-write the inode */
	memset(&inode, 0, sizeof(inode));
	err = ext2fs_read_inode_full(fs, ino, (struct ext2_inode *)&inode,
						 sizeof(inode));
	if (err)
		return translate_error(fs, ino, err);

	increment_version((struct ext2_inode *) &inode);
	EXT4_INODE_SET_XTIME(i_ctime, &now, &inode);

	err = ext2fs_write_inode_full(fs, ino, (struct ext2_inode *)&inode,
							sizeof(inode));
	if (err)
		return translate_error(fs, ino, err);

	return 0;
}

static int update_atime(ext2_filsys fs, ext2_ino_t ino) {
	errcode_t err;
	struct ext2_inode_large inode, *pinode;
	struct timespec atime, mtime, now;

	if (!(fs->flags & EXT2_FLAG_RW))
		return 0;
	memset(&inode, 0, sizeof(inode));
	err = ext2fs_read_inode_full(fs, ino, (struct ext2_inode *)&inode,
						 sizeof(inode));
	if (err)
		return translate_error(fs, ino, err);

	pinode = &inode;
	EXT4_INODE_GET_XTIME(i_atime, &atime, pinode);
	EXT4_INODE_GET_XTIME(i_mtime, &mtime, pinode);
	get_now(&now);
	/*
	 * If atime is newer than mtime and atime hasn't been updated in thirty
	 * seconds, skip the atime update.	Same idea as Linux "relatime".
	 */
	if (atime.tv_sec >= mtime.tv_sec && atime.tv_sec >= now.tv_sec - 30)
		return 0;
	EXT4_INODE_SET_XTIME(i_atime, &now, &inode);

	err = ext2fs_write_inode_full(fs, ino, (struct ext2_inode *)&inode,
							sizeof(inode));
	if (err)
		return translate_error(fs, ino, err);

	return 0;
}

static int update_mtime(ext2_filsys fs, ext2_ino_t ino,
			struct ext2_inode_large *pinode) {
	errcode_t err;
	struct ext2_inode_large inode;
	struct timespec now;

	if (pinode) {
		get_now(&now);
		EXT4_INODE_SET_XTIME(i_mtime, &now, pinode);
		EXT4_INODE_SET_XTIME(i_ctime, &now, pinode);
		increment_version((struct ext2_inode *) pinode);
		return 0;
	}

	memset(&inode, 0, sizeof(inode));
	err = ext2fs_read_inode_full(fs, ino, (struct ext2_inode *)&inode,
						 sizeof(inode));
	if (err)
		return translate_error(fs, ino, err);

	get_now(&now);
	EXT4_INODE_SET_XTIME(i_mtime, &now, &inode);
	EXT4_INODE_SET_XTIME(i_ctime, &now, &inode);
	increment_version((struct ext2_inode *) &inode);

	err = ext2fs_write_inode_full(fs, ino, (struct ext2_inode *)&inode,
							sizeof(inode));
	if (err)
		return translate_error(fs, ino, err);

	return 0;
}

static int ext2_file_type(unsigned int mode) {
	if (LINUX_S_ISREG(mode))
		return EXT2_FT_REG_FILE;

	if (LINUX_S_ISDIR(mode))
		return EXT2_FT_DIR;

	if (LINUX_S_ISCHR(mode))
		return EXT2_FT_CHRDEV;

	if (LINUX_S_ISBLK(mode))
		return EXT2_FT_BLKDEV;

	if (LINUX_S_ISLNK(mode))
		return EXT2_FT_SYMLINK;

	if (LINUX_S_ISFIFO(mode))
		return EXT2_FT_FIFO;

	if (LINUX_S_ISSOCK(mode))
		return EXT2_FT_SOCK;

	return 0;
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

double node_ext2fs_stat_i_size(ext2_file_t file) {
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
	free(data);
	if (ret) return ret;
	return discard(disk_id, channel->block_size, block, count);
}

struct struct_io_manager js_io_manager;

io_manager get_js_io_manager() {
	js_io_manager.magic						=	EXT2_ET_MAGIC_IO_MANAGER;
	js_io_manager.name						 =	"JavaScript IO Manager";
	js_io_manager.open						 =	js_open_entry;
	js_io_manager.close						=	js_close_entry;
	js_io_manager.set_blksize			=	set_blksize;
	js_io_manager.read_blk				 =	js_read_blk_entry;
	js_io_manager.write_blk				=	js_write_blk_entry;
	js_io_manager.flush						=	js_flush_entry;
	js_io_manager.read_blk64			 =	js_read_blk64_entry;
	js_io_manager.write_blk64			=	js_write_blk64_entry;
	js_io_manager.discard					=	js_discard_entry;
	js_io_manager.cache_readahead	=	js_cache_readahead_entry;
	js_io_manager.zeroout					=	js_zeroout_entry;
	return &js_io_manager;
};

static int __translate_error(ext2_filsys fs, errcode_t err, ext2_ino_t ino,
					 const char *file, int line)
{
	struct timespec now;
	int ret = err;
	int is_err = 0;

	int disk_id = get_disk_id(fs->io);

	/* Translate ext2 error to unix error code */
	if (err < EXT2_ET_BASE)
		goto no_translation;
	switch (err) {
	case EXT2_ET_NO_MEMORY:
	case EXT2_ET_TDB_ERR_OOM:
		ret = -ENOMEM;
		break;
	case EXT2_ET_INVALID_ARGUMENT:
	case EXT2_ET_LLSEEK_FAILED:
		ret = -EINVAL;
		break;
	case EXT2_ET_NO_DIRECTORY:
		ret = -ENOTDIR;
		break;
	case EXT2_ET_FILE_NOT_FOUND:
		ret = -ENOENT;
		break;
	case EXT2_ET_DIR_NO_SPACE:
		is_err = 1;
		/* fallthrough */
	case EXT2_ET_TOOSMALL:
	case EXT2_ET_BLOCK_ALLOC_FAIL:
	case EXT2_ET_INODE_ALLOC_FAIL:
	case EXT2_ET_EA_NO_SPACE:
		ret = -ENOSPC;
		break;
	case EXT2_ET_SYMLINK_LOOP:
		ret = -EMLINK;
		break;
	case EXT2_ET_FILE_TOO_BIG:
		ret = -EFBIG;
		break;
	case EXT2_ET_TDB_ERR_EXISTS:
	case EXT2_ET_FILE_EXISTS:
		ret = -EEXIST;
		break;
	case EXT2_ET_MMP_FAILED:
	case EXT2_ET_MMP_FSCK_ON:
		ret = -EBUSY;
		break;
	case EXT2_ET_EA_KEY_NOT_FOUND:
#ifdef ENODATA
		ret = -ENODATA;
#else
		ret = -ENOENT;
#endif
		break;
	/* Sometimes fuse returns a garbage file handle pointer to us... */
	case EXT2_ET_MAGIC_EXT2_FILE:
		ret = -EFAULT;
		break;
	case EXT2_ET_UNIMPLEMENTED:
		ret = -EOPNOTSUPP;
		break;
	default:
		is_err = 1;
		ret = -EIO;
		break;
	}

no_translation:
	if (!is_err)
		return ret;

	if (ino)
		fprintf(stderr, "node_ext2fs (%d): (inode #%d) at %s:%d.\n", disk_id, ino, file, line);
	else
		fprintf(stderr, "node_ext2fs (%d): at %s:%d.\n", disk_id, file, line);

	ext2fs_mark_super_dirty(fs);
	ext2fs_flush(fs);

	return ret;
}