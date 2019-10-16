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
// __u64 is defined in /usr/include/asm-generic/int-ll64.h on arm64
#if !defined(__aarch64__)
#ifdef __linux__
#include <linux/types.h>
#else
typedef uint64_t __u64;
#endif
#endif

typedef __u64 ext2_off64_t;

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
#define EXT2_SEEK_CUR 1
#define EXT2_SEEK_END 2


#define EXT2_FILE_WRITE		0x0001
#define EXT2_FILE_CREATE	0x0002

#define EXT2_ET_DIR_NO_SPACE (2133571366L)

#define EXT2_FT_REG_FILE 1

#define EXT4_INLINE_DATA_FL		0x10000000 /* Inode has inline data */
#define EXT4_EXTENTS_FL 		0x00080000 /* Inode uses extents */

#define EXT2_SB(sb)	(sb)
/*
 * Ext2/linux mode flags.  We define them here so that we don't need
 * to depend on the OS's sys/stat.h, since we may be compiling on a
 * non-Linux system.
 */
#define LINUX_S_IFMT  00170000
#define LINUX_S_IFSOCK 0140000
#define LINUX_S_IFLNK	 0120000
#define LINUX_S_IFREG  0100000
#define LINUX_S_IFBLK  0060000
#define LINUX_S_IFDIR  0040000
#define LINUX_S_IFCHR  0020000
#define LINUX_S_IFIFO  0010000
#define LINUX_S_ISUID  0004000
#define LINUX_S_ISGID  0002000
#define LINUX_S_ISVTX  0001000

#define LINUX_S_IRWXU 00700
#define LINUX_S_IRUSR 00400
#define LINUX_S_IWUSR 00200
#define LINUX_S_IXUSR 00100

#define LINUX_S_IRWXG 00070
#define LINUX_S_IRGRP 00040
#define LINUX_S_IWGRP 00020
#define LINUX_S_IXGRP 00010

#define LINUX_S_IRWXO 00007
#define LINUX_S_IROTH 00004
#define LINUX_S_IWOTH 00002
#define LINUX_S_IXOTH 00001

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

#define EXT2_NAME_LEN 255

struct ext2_dir_entry {
	__u32	inode;			/* Inode number */
	__u16	rec_len;		/* Directory entry length */
	__u16	name_len;		/* Name length */
	char	name[EXT2_NAME_LEN];	/* File name */
};

extern int ext2fs_dirent_name_len(const struct ext2_dir_entry *entry);

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

typedef struct ext2_extent_handle *ext2_extent_handle_t;

extern errcode_t ext2fs_check_directory(ext2_filsys fs, ext2_ino_t ino);

extern void ext2fs_extent_free(ext2_extent_handle_t handle);

extern errcode_t ext2fs_write_new_inode(
	ext2_filsys fs,
	ext2_ino_t ino,
	struct ext2_inode * inode
);

extern errcode_t ext2fs_inline_data_init(ext2_filsys fs, ext2_ino_t ino);

errcode_t ext2fs_dir_iterate(
	ext2_filsys fs,
	ext2_ino_t dir,
	int flags,
	char *block_buf,
	int (*func)(
		struct ext2_dir_entry *dirent,
		int offset,
		int blocksize,
		char *buf,
		void *priv_data
	),
	void *priv_data
);

extern errcode_t ext2fs_file_read(
	ext2_file_t file,
	void *buf,
	unsigned int wanted,
	unsigned int *got
);

extern errcode_t ext2fs_file_write(
	ext2_file_t file,
	const void *buf,
	unsigned int nbytes,
	unsigned int *written
);

extern errcode_t ext2fs_file_open(
	ext2_filsys fs,
	ext2_ino_t ino,
	int flags,
	ext2_file_t *ret
);

extern errcode_t ext2fs_file_llseek(
	ext2_file_t file,
	__u64 offset,
	int whence,
	__u64 *ret_pos
);

extern errcode_t ext2fs_new_inode(
	ext2_filsys fs,
	ext2_ino_t dir,
	int mode,
	ext2fs_inode_bitmap map,
	ext2_ino_t *ret
);

errcode_t ext2fs_link(
	ext2_filsys fs,
	ext2_ino_t dir,
	const char *name,
	ext2_ino_t ino,
	int flags
);

extern errcode_t ext2fs_expand_dir(ext2_filsys fs, ext2_ino_t dir);

extern int ext2fs_test_inode_bitmap2(
	ext2fs_inode_bitmap bitmap,
	ext2_ino_t inode
);

void ext2fs_inode_alloc_stats2(
	ext2_filsys fs,
	ext2_ino_t ino,
	int inuse,
	int isdir
);

extern errcode_t ext2fs_inode_size_set(
	ext2_filsys fs,
	struct ext2_inode *inode,
	ext2_off64_t size
);

extern errcode_t ext2fs_read_bitmaps(ext2_filsys fs);

extern errcode_t ext2fs_file_close(ext2_file_t file);

extern errcode_t ext2fs_extent_open2(
	ext2_filsys fs,
	ext2_ino_t ino,
	struct ext2_inode *inode,
	ext2_extent_handle_t *ret_handle
);

extern errcode_t ext2fs_read_inode(
	ext2_filsys fs,
	ext2_ino_t ino,
	struct ext2_inode * inode
);

extern errcode_t ext2fs_write_inode(
	ext2_filsys fs,
	ext2_ino_t ino,
	struct ext2_inode * inode
);

errcode_t ext2fs_unlink(
	ext2_filsys fs,
	ext2_ino_t dir,
	const char *name,
	ext2_ino_t ino,
	int flags
);

extern errcode_t ext2fs_mkdir(
	ext2_filsys fs,
	ext2_ino_t parent,
	ext2_ino_t inum,
	const char *name
);

extern errcode_t ext2fs_file_set_size2(ext2_file_t file, ext2_off64_t size);

struct ext2_super_block {
	uint32_t	s_inodes_count;		/* Inodes count */
	uint32_t	s_blocks_count;		/* Blocks count */
	uint32_t	s_r_blocks_count;	/* Reserved blocks count */
	uint32_t	s_free_blocks_count;	/* Free blocks count */
	uint32_t	s_free_inodes_count;	/* Free inodes count */
	uint32_t	s_first_data_block;	/* First Data Block */
	uint32_t	s_feature_compat;	/* compatible feature set */
	uint32_t	s_feature_incompat;	/* incompatible feature set */
	uint32_t	s_feature_ro_compat;	/* readonly-compatible feature set */
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
extern errcode_t ext2fs_close_free(ext2_filsys *fs);

errcode_t ext2fs_read_block_bitmap(ext2_filsys fs);

int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);

/*extern errcode_t ext2fs_file_open(ext2_filsys fs, ino_t ino, int flags, ext2_file_t *ret);*/

errcode_t io_channel_discard(io_channel channel, unsigned long long block, unsigned long long count);

#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INODE	0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT2_FEATURE_COMPAT_LAZY_BG		0x0040
/* #define EXT2_FEATURE_COMPAT_EXCLUDE_INODE	0x0080 not used, legacy */
#define EXT2_FEATURE_COMPAT_EXCLUDE_BITMAP	0x0100
#define EXT4_FEATURE_COMPAT_SPARSE_SUPER2	0x0200


#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
/* #define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004 not used */
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE	0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040
#define EXT4_FEATURE_RO_COMPAT_HAS_SNAPSHOT	0x0080
#define EXT4_FEATURE_RO_COMPAT_QUOTA		0x0100
#define EXT4_FEATURE_RO_COMPAT_BIGALLOC		0x0200
/*
 * METADATA_CSUM implies GDT_CSUM.  When METADATA_CSUM is set, group
 * descriptor checksums use the same algorithm as all other data
 * structures' checksums.
 */
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM	0x0400
#define EXT4_FEATURE_RO_COMPAT_REPLICA		0x0800
#define EXT4_FEATURE_RO_COMPAT_READONLY		0x1000
#define EXT4_FEATURE_RO_COMPAT_PROJECT		0x2000 /* Project quota */


#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004 /* Needs recovery */
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008 /* Journal device */
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT3_FEATURE_INCOMPAT_EXTENTS		0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200
#define EXT4_FEATURE_INCOMPAT_EA_INODE		0x0400
#define EXT4_FEATURE_INCOMPAT_DIRDATA		0x1000
#define EXT4_FEATURE_INCOMPAT_CSUM_SEED		0x2000
#define EXT4_FEATURE_INCOMPAT_LARGEDIR		0x4000 /* >2GB or 3-lvl htree */
#define EXT4_FEATURE_INCOMPAT_INLINE_DATA	0x8000 /* data in inode */
#define EXT4_FEATURE_INCOMPAT_ENCRYPT		0x10000

#define EXT4_FEATURE_COMPAT_FUNCS(name, ver, flagname) \
static inline int ext2fs_has_feature_##name(struct ext2_super_block *sb) \
{ \
	return ((EXT2_SB(sb)->s_feature_compat & \
		 EXT##ver##_FEATURE_COMPAT_##flagname) != 0); \
} \
static inline void ext2fs_set_feature_##name(struct ext2_super_block *sb) \
{ \
	EXT2_SB(sb)->s_feature_compat |= \
		EXT##ver##_FEATURE_COMPAT_##flagname; \
} \
static inline void ext2fs_clear_feature_##name(struct ext2_super_block *sb) \
{ \
	EXT2_SB(sb)->s_feature_compat &= \
		~EXT##ver##_FEATURE_COMPAT_##flagname; \
}

#define EXT4_FEATURE_RO_COMPAT_FUNCS(name, ver, flagname) \
static inline int ext2fs_has_feature_##name(struct ext2_super_block *sb) \
{ \
	return ((EXT2_SB(sb)->s_feature_ro_compat & \
		 EXT##ver##_FEATURE_RO_COMPAT_##flagname) != 0); \
} \
static inline void ext2fs_set_feature_##name(struct ext2_super_block *sb) \
{ \
	EXT2_SB(sb)->s_feature_ro_compat |= \
		EXT##ver##_FEATURE_RO_COMPAT_##flagname; \
} \
static inline void ext2fs_clear_feature_##name(struct ext2_super_block *sb) \
{ \
	EXT2_SB(sb)->s_feature_ro_compat &= \
		~EXT##ver##_FEATURE_RO_COMPAT_##flagname; \
}

#define EXT4_FEATURE_INCOMPAT_FUNCS(name, ver, flagname) \
static inline int ext2fs_has_feature_##name(struct ext2_super_block *sb) \
{ \
	return ((EXT2_SB(sb)->s_feature_incompat & \
		 EXT##ver##_FEATURE_INCOMPAT_##flagname) != 0); \
} \
static inline void ext2fs_set_feature_##name(struct ext2_super_block *sb) \
{ \
	EXT2_SB(sb)->s_feature_incompat |= \
		EXT##ver##_FEATURE_INCOMPAT_##flagname; \
} \
static inline void ext2fs_clear_feature_##name(struct ext2_super_block *sb) \
{ \
	EXT2_SB(sb)->s_feature_incompat &= \
		~EXT##ver##_FEATURE_INCOMPAT_##flagname; \
}

EXT4_FEATURE_COMPAT_FUNCS(dir_prealloc,		2, DIR_PREALLOC)
EXT4_FEATURE_COMPAT_FUNCS(imagic_inodes,	2, IMAGIC_INODES)
EXT4_FEATURE_COMPAT_FUNCS(journal,		3, HAS_JOURNAL)
EXT4_FEATURE_COMPAT_FUNCS(xattr,		2, EXT_ATTR)
EXT4_FEATURE_COMPAT_FUNCS(resize_inode,		2, RESIZE_INODE)
EXT4_FEATURE_COMPAT_FUNCS(dir_index,		2, DIR_INDEX)
EXT4_FEATURE_COMPAT_FUNCS(lazy_bg,		2, LAZY_BG)
EXT4_FEATURE_COMPAT_FUNCS(exclude_bitmap,	2, EXCLUDE_BITMAP)
EXT4_FEATURE_COMPAT_FUNCS(sparse_super2,	4, SPARSE_SUPER2)

EXT4_FEATURE_RO_COMPAT_FUNCS(sparse_super,	2, SPARSE_SUPER)
EXT4_FEATURE_RO_COMPAT_FUNCS(large_file,	2, LARGE_FILE)
EXT4_FEATURE_RO_COMPAT_FUNCS(huge_file,		4, HUGE_FILE)
EXT4_FEATURE_RO_COMPAT_FUNCS(gdt_csum,		4, GDT_CSUM)
EXT4_FEATURE_RO_COMPAT_FUNCS(dir_nlink,		4, DIR_NLINK)
EXT4_FEATURE_RO_COMPAT_FUNCS(extra_isize,	4, EXTRA_ISIZE)
EXT4_FEATURE_RO_COMPAT_FUNCS(has_snapshot,	4, HAS_SNAPSHOT)
EXT4_FEATURE_RO_COMPAT_FUNCS(quota,		4, QUOTA)
EXT4_FEATURE_RO_COMPAT_FUNCS(bigalloc,		4, BIGALLOC)
EXT4_FEATURE_RO_COMPAT_FUNCS(metadata_csum,	4, METADATA_CSUM)
EXT4_FEATURE_RO_COMPAT_FUNCS(replica,		4, REPLICA)
EXT4_FEATURE_RO_COMPAT_FUNCS(readonly,		4, READONLY)
EXT4_FEATURE_RO_COMPAT_FUNCS(project,		4, PROJECT)

EXT4_FEATURE_INCOMPAT_FUNCS(compression,	2, COMPRESSION)
EXT4_FEATURE_INCOMPAT_FUNCS(filetype,		2, FILETYPE)
EXT4_FEATURE_INCOMPAT_FUNCS(journal_needs_recovery,	3, RECOVER)
EXT4_FEATURE_INCOMPAT_FUNCS(journal_dev,	3, JOURNAL_DEV)
EXT4_FEATURE_INCOMPAT_FUNCS(meta_bg,		2, META_BG)
EXT4_FEATURE_INCOMPAT_FUNCS(extents,		3, EXTENTS)
EXT4_FEATURE_INCOMPAT_FUNCS(64bit,		4, 64BIT)
EXT4_FEATURE_INCOMPAT_FUNCS(mmp,		4, MMP)
EXT4_FEATURE_INCOMPAT_FUNCS(flex_bg,		4, FLEX_BG)
EXT4_FEATURE_INCOMPAT_FUNCS(ea_inode,		4, EA_INODE)
EXT4_FEATURE_INCOMPAT_FUNCS(dirdata,		4, DIRDATA)
EXT4_FEATURE_INCOMPAT_FUNCS(csum_seed,		4, CSUM_SEED)
EXT4_FEATURE_INCOMPAT_FUNCS(largedir,		4, LARGEDIR)
EXT4_FEATURE_INCOMPAT_FUNCS(inline_data,	4, INLINE_DATA)
EXT4_FEATURE_INCOMPAT_FUNCS(encrypt,		4, ENCRYPT)

static void increment_version(struct ext2_inode *inode) {
	inode->osd1.linux1.l_i_version++;
}

#ifdef __cplusplus
}
#endif

#define __EXT2FS_H
#endif /* !defined(__EXT2FS_H) */
