#if !defined(__EXT2FS_H)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef long     errcode_t;
typedef uint32_t dgrp_t;
typedef uint32_t blk_t;

#define EXT2_FLAG_RW 0x01
#define EXT2_ET_MAGIC_IO_CHANNEL 2133571333L;
#define EXT2_ET_MAGIC_IO_MANAGER 2133571335L;

typedef void* io_stats;
typedef void* ext2fs_inode_bitmap;
typedef void* ext2fs_block_bitmap;

typedef struct struct_ext2_filsys *ext2_filsys;
typedef struct struct_io_manager *io_manager;
typedef struct struct_io_channel *io_channel;

struct ext2_super_block {
	uint32_t	s_inodes_count;		/* Inodes count */
	uint32_t	s_blocks_count;		/* Blocks count */
	uint32_t	s_r_blocks_count;	/* Reserved blocks count */
	uint32_t	s_free_blocks_count;	/* Free blocks count */
	uint32_t	s_free_inodes_count;	/* Free inodes count */
	uint32_t	s_first_data_block;	/* First Data Block */
};

struct struct_ext2_filsys {
	errcode_t               magic;
	io_channel              io;
	int                     flags;
	char                    *device_name;
	struct ext2_super_block *super;
	unsigned int            blocksize;
	int                     fragsize;
	dgrp_t                  group_desc_count;
	unsigned long           desc_blocks;
	void                    *group_desc;
	unsigned int            inode_blocks_per_group;
	ext2fs_inode_bitmap     inode_map;
	ext2fs_block_bitmap     block_map;
};

struct struct_io_channel {
	errcode_t	magic;
	io_manager	manager;
	char		*name;
	int		block_size;
	errcode_t	(*read_error)(io_channel channel,
				      unsigned long block,
				      int count,
				      void *data,
				      size_t size,
				      int actual_bytes_read,
				      errcode_t	error);
	errcode_t	(*write_error)(io_channel channel,
				       unsigned long block,
				       int count,
				       const void *data,
				       size_t size,
				       int actual_bytes_written,
				       errcode_t error);
	int		refcount;
	int		flags;
	long		reserved[14];
	void		*private_data;
	void		*app_data;
	int		align;
};

struct struct_io_manager {
	errcode_t magic;
	const char *name;
	errcode_t (*open)(const char *name, int flags, io_channel *channel);
	errcode_t (*close)(io_channel channel);
	errcode_t (*set_blksize)(io_channel channel, int blksize);
	errcode_t (*read_blk)(io_channel channel, unsigned long block, int count, void *data);
	errcode_t (*write_blk)(io_channel channel, unsigned long block, int count, const void *data);
	errcode_t (*flush)(io_channel channel);
	errcode_t (*write_byte)(io_channel channel, unsigned long offset, int count, const void *data);
	errcode_t (*set_option)(io_channel channel, const char *option, const char *arg);
	errcode_t (*get_stats)(io_channel channel, io_stats *io_stats);
	errcode_t (*read_blk64)(io_channel channel, unsigned long long block, int count, void *data);
	errcode_t (*write_blk64)(io_channel channel, unsigned long long block, int count, const void *data);
	errcode_t (*discard)(io_channel channel, unsigned long long block, unsigned long long count);
	errcode_t (*cache_readahead)(io_channel channel, unsigned long long block, unsigned long long count);
	errcode_t (*zeroout)(io_channel channel, unsigned long long block, unsigned long long count);
	long	reserved[14];
};

errcode_t ext2fs_close(ext2_filsys fs);

errcode_t ext2fs_open(const char *name, int flags, int superblock, unsigned int block_size, io_manager manager, ext2_filsys *ret_fs);

errcode_t ext2fs_read_block_bitmap(ext2_filsys fs);

int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);

errcode_t io_channel_discard(io_channel channel, unsigned long long block, unsigned long long count);

#ifdef __cplusplus
}
#endif

#define __EXT2FS_H
#endif /* !defined(__EXT2FS_H) */
