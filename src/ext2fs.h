#if !defined(__EXT2FS_H)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef long     errcode_t;
typedef uint32_t dgrp_t;
typedef uint32_t blk_t;
typedef uint32_t ext2_ino_t;
typedef uint32_t ext2_off_t;

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;

/*
 * Constants relative to the data blocks
 */
#define EXT2_NDIR_BLOCKS		12
#define EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

#define EXT2_FLAG_RW 0x01
#define EXT2_ET_MAGIC_IO_CHANNEL 2133571333L
#define EXT2_ET_MAGIC_IO_MANAGER 2133571335L
#define EXT2_ROOT_INO 2
#define EXT2_SEEK_SET 0

typedef void* io_stats;
typedef void* ext2fs_inode_bitmap;
typedef void* ext2fs_block_bitmap;

typedef struct struct_ext2_filsys *ext2_filsys;
typedef struct struct_io_manager *io_manager;
typedef struct struct_io_channel *io_channel;

extern errcode_t ext2fs_namei(
	ext2_filsys fs,
	ext2_ino_t root,
	ext2_ino_t cwd,
	const char *name,
	ext2_ino_t *inode
);

#define EXT2_ET_NO_MEMORY (2133571398L)

extern errcode_t ext2fs_get_mem(unsigned long size, void *ptr);
extern errcode_t ext2fs_free_mem(void *ptr);

struct ext2_inode {
	__u16	i_mode;		/* File mode */
	__u16	i_uid;		/* Low 16 bits of Owner Uid */
	__u32	i_size;		/* Size in bytes */
	__u32	i_atime;	/* Access time */
	__u32	i_ctime;	/* Inode change time */
	__u32	i_mtime;	/* Modification time */
	__u32	i_dtime;	/* Deletion Time */
	__u16	i_gid;		/* Low 16 bits of Group Id */
	__u16	i_links_count;	/* Links count */
	__u32	i_blocks;	/* Blocks count */
	__u32	i_flags;	/* File flags */
	union {
		struct {
			__u32	l_i_version; /* was l_i_reserved1 */
		} linux1;
		struct {
			__u32  h_i_translator;
		} hurd1;
	} osd1;				/* OS dependent 1 */
	__u32	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
	__u32	i_generation;	/* File version (for NFS) */
	__u32	i_file_acl;	/* File ACL */
	__u32	i_size_high;
	__u32	i_faddr;	/* Fragment address */
	union {
		struct {
			__u16	l_i_blocks_hi;
			__u16	l_i_file_acl_high;
			__u16	l_i_uid_high;	/* these 2 fields    */
			__u16	l_i_gid_high;	/* were reserved2[0] */
			__u16	l_i_checksum_lo; /* crc32c(uuid+inum+inode) */
			__u16	l_i_reserved;
		} linux2;
		struct {
			__u8	h_i_frag;	/* Fragment number */
			__u8	h_i_fsize;	/* Fragment size */
			__u16	h_i_mode_high;
			__u16	h_i_uid_high;
			__u16	h_i_gid_high;
			__u32	h_i_author;
		} hurd2;
	} osd2;				/* OS dependent 2 */
};

struct ext2_file {
	errcode_t magic;
	ext2_filsys fs;
	ext2_ino_t ino;
	struct ext2_inode inode;
	int flags;
	unsigned long long pos;
	unsigned long long blockno;
	unsigned long long physblock;
	char *buf;
};
typedef struct ext2_file *ext2_file_t;

extern errcode_t ext2fs_file_read(
	ext2_file_t file,
	void *buf,
	unsigned int wanted,
	unsigned int *got
);

extern errcode_t ext2fs_file_open(
	ext2_filsys fs,
	ext2_ino_t ino,
	int flags,
	ext2_file_t *ret
);

extern errcode_t ext2fs_file_lseek(
	ext2_file_t file,
	ext2_off_t offset,
	int whence,
	ext2_off_t *ret_pos
);

extern errcode_t ext2fs_file_close(ext2_file_t file);

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

errcode_t ext2fs_open(const char *name, int flags, int superblock, unsigned int block_size, io_manager manager, ext2_filsys *ret_fs);
errcode_t ext2fs_close(ext2_filsys fs);

errcode_t ext2fs_read_block_bitmap(ext2_filsys fs);

int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);

/*extern errcode_t ext2fs_file_open(ext2_filsys fs, ino_t ino, int flags, ext2_file_t *ret);*/

errcode_t io_channel_discard(io_channel channel, unsigned long long block, unsigned long long count);

#ifdef __cplusplus
}
#endif

#define __EXT2FS_H
#endif /* !defined(__EXT2FS_H) */
