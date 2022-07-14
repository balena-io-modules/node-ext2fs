#if !defined(__EXT2FS_H)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef long     errcode_t;
typedef uint32_t dgrp_t;
typedef uint32_t blk_t;
typedef uint64_t blk64_t;
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
typedef	int32_t	__s32;
#endif
#endif

typedef __u64 ext2_off64_t;

/*
 * Constants relative to the data blocks
 */
#define EXT2_NDIR_BLOCKS    12
#define EXT2_IND_BLOCK      EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK      (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK      (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS      (EXT2_TIND_BLOCK + 1)

#define EXT2_FLAG_RW 0x01
#define EXT2_ROOT_INO 2
#define EXT2_SEEK_SET 0
#define EXT2_SEEK_CUR 1
#define EXT2_SEEK_END 2

#define EXT2_ET_BASE                             (2133571328L)
#define EXT2_ET_MAGIC_EXT2FS_FILSYS              (2133571329L)
#define EXT2_ET_MAGIC_BADBLOCKS_LIST             (2133571330L)
#define EXT2_ET_MAGIC_BADBLOCKS_ITERATE          (2133571331L)
#define EXT2_ET_MAGIC_INODE_SCAN                 (2133571332L)
#define EXT2_ET_MAGIC_IO_CHANNEL                 (2133571333L)
#define EXT2_ET_MAGIC_UNIX_IO_CHANNEL            (2133571334L)
#define EXT2_ET_MAGIC_IO_MANAGER                 (2133571335L)
#define EXT2_ET_MAGIC_BLOCK_BITMAP               (2133571336L)
#define EXT2_ET_MAGIC_INODE_BITMAP               (2133571337L)
#define EXT2_ET_MAGIC_GENERIC_BITMAP             (2133571338L)
#define EXT2_ET_MAGIC_TEST_IO_CHANNEL            (2133571339L)
#define EXT2_ET_MAGIC_DBLIST                     (2133571340L)
#define EXT2_ET_MAGIC_ICOUNT                     (2133571341L)
#define EXT2_ET_MAGIC_PQ_IO_CHANNEL              (2133571342L)
#define EXT2_ET_MAGIC_EXT2_FILE                  (2133571343L)
#define EXT2_ET_MAGIC_E2IMAGE                    (2133571344L)
#define EXT2_ET_MAGIC_INODE_IO_CHANNEL           (2133571345L)
#define EXT2_ET_MAGIC_EXTENT_HANDLE              (2133571346L)
#define EXT2_ET_BAD_MAGIC                        (2133571347L)
#define EXT2_ET_REV_TOO_HIGH                     (2133571348L)
#define EXT2_ET_RO_FILSYS                        (2133571349L)
#define EXT2_ET_GDESC_READ                       (2133571350L)
#define EXT2_ET_GDESC_WRITE                      (2133571351L)
#define EXT2_ET_GDESC_BAD_BLOCK_MAP              (2133571352L)
#define EXT2_ET_GDESC_BAD_INODE_MAP              (2133571353L)
#define EXT2_ET_GDESC_BAD_INODE_TABLE            (2133571354L)
#define EXT2_ET_INODE_BITMAP_WRITE               (2133571355L)
#define EXT2_ET_INODE_BITMAP_READ                (2133571356L)
#define EXT2_ET_BLOCK_BITMAP_WRITE               (2133571357L)
#define EXT2_ET_BLOCK_BITMAP_READ                (2133571358L)
#define EXT2_ET_INODE_TABLE_WRITE                (2133571359L)
#define EXT2_ET_INODE_TABLE_READ                 (2133571360L)
#define EXT2_ET_NEXT_INODE_READ                  (2133571361L)
#define EXT2_ET_UNEXPECTED_BLOCK_SIZE            (2133571362L)
#define EXT2_ET_DIR_CORRUPTED                    (2133571363L)
#define EXT2_ET_SHORT_READ                       (2133571364L)
#define EXT2_ET_SHORT_WRITE                      (2133571365L)
#define EXT2_ET_DIR_NO_SPACE                     (2133571366L)
#define EXT2_ET_NO_INODE_BITMAP                  (2133571367L)
#define EXT2_ET_NO_BLOCK_BITMAP                  (2133571368L)
#define EXT2_ET_BAD_INODE_NUM                    (2133571369L)
#define EXT2_ET_BAD_BLOCK_NUM                    (2133571370L)
#define EXT2_ET_EXPAND_DIR_ERR                   (2133571371L)
#define EXT2_ET_TOOSMALL                         (2133571372L)
#define EXT2_ET_BAD_BLOCK_MARK                   (2133571373L)
#define EXT2_ET_BAD_BLOCK_UNMARK                 (2133571374L)
#define EXT2_ET_BAD_BLOCK_TEST                   (2133571375L)
#define EXT2_ET_BAD_INODE_MARK                   (2133571376L)
#define EXT2_ET_BAD_INODE_UNMARK                 (2133571377L)
#define EXT2_ET_BAD_INODE_TEST                   (2133571378L)
#define EXT2_ET_FUDGE_BLOCK_BITMAP_END           (2133571379L)
#define EXT2_ET_FUDGE_INODE_BITMAP_END           (2133571380L)
#define EXT2_ET_BAD_IND_BLOCK                    (2133571381L)
#define EXT2_ET_BAD_DIND_BLOCK                   (2133571382L)
#define EXT2_ET_BAD_TIND_BLOCK                   (2133571383L)
#define EXT2_ET_NEQ_BLOCK_BITMAP                 (2133571384L)
#define EXT2_ET_NEQ_INODE_BITMAP                 (2133571385L)
#define EXT2_ET_BAD_DEVICE_NAME                  (2133571386L)
#define EXT2_ET_MISSING_INODE_TABLE              (2133571387L)
#define EXT2_ET_CORRUPT_SUPERBLOCK               (2133571388L)
#define EXT2_ET_BAD_GENERIC_MARK                 (2133571389L)
#define EXT2_ET_BAD_GENERIC_UNMARK               (2133571390L)
#define EXT2_ET_BAD_GENERIC_TEST                 (2133571391L)
#define EXT2_ET_SYMLINK_LOOP                     (2133571392L)
#define EXT2_ET_CALLBACK_NOTHANDLED              (2133571393L)
#define EXT2_ET_BAD_BLOCK_IN_INODE_TABLE         (2133571394L)
#define EXT2_ET_UNSUPP_FEATURE                   (2133571395L)
#define EXT2_ET_RO_UNSUPP_FEATURE                (2133571396L)
#define EXT2_ET_LLSEEK_FAILED                    (2133571397L)
#define EXT2_ET_NO_MEMORY                        (2133571398L)
#define EXT2_ET_INVALID_ARGUMENT                 (2133571399L)
#define EXT2_ET_BLOCK_ALLOC_FAIL                 (2133571400L)
#define EXT2_ET_INODE_ALLOC_FAIL                 (2133571401L)
#define EXT2_ET_NO_DIRECTORY                     (2133571402L)
#define EXT2_ET_TOO_MANY_REFS                    (2133571403L)
#define EXT2_ET_FILE_NOT_FOUND                   (2133571404L)
#define EXT2_ET_FILE_RO                          (2133571405L)
#define EXT2_ET_DB_NOT_FOUND                     (2133571406L)
#define EXT2_ET_DIR_EXISTS                       (2133571407L)
#define EXT2_ET_UNIMPLEMENTED                    (2133571408L)
#define EXT2_ET_CANCEL_REQUESTED                 (2133571409L)
#define EXT2_ET_FILE_TOO_BIG                     (2133571410L)
#define EXT2_ET_JOURNAL_NOT_BLOCK                (2133571411L)
#define EXT2_ET_NO_JOURNAL_SB                    (2133571412L)
#define EXT2_ET_JOURNAL_TOO_SMALL                (2133571413L)
#define EXT2_ET_JOURNAL_UNSUPP_VERSION           (2133571414L)
#define EXT2_ET_LOAD_EXT_JOURNAL                 (2133571415L)
#define EXT2_ET_NO_JOURNAL                       (2133571416L)
#define EXT2_ET_DIRHASH_UNSUPP                   (2133571417L)
#define EXT2_ET_BAD_EA_BLOCK_NUM                 (2133571418L)
#define EXT2_ET_TOO_MANY_INODES                  (2133571419L)
#define EXT2_ET_NOT_IMAGE_FILE                   (2133571420L)
#define EXT2_ET_RES_GDT_BLOCKS                   (2133571421L)
#define EXT2_ET_RESIZE_INODE_CORRUPT             (2133571422L)
#define EXT2_ET_SET_BMAP_NO_IND                  (2133571423L)
#define EXT2_ET_TDB_SUCCESS                      (2133571424L)
#define EXT2_ET_TDB_ERR_CORRUPT                  (2133571425L)
#define EXT2_ET_TDB_ERR_IO                       (2133571426L)
#define EXT2_ET_TDB_ERR_LOCK                     (2133571427L)
#define EXT2_ET_TDB_ERR_OOM                      (2133571428L)
#define EXT2_ET_TDB_ERR_EXISTS                   (2133571429L)
#define EXT2_ET_TDB_ERR_NOLOCK                   (2133571430L)
#define EXT2_ET_TDB_ERR_EINVAL                   (2133571431L)
#define EXT2_ET_TDB_ERR_NOEXIST                  (2133571432L)
#define EXT2_ET_TDB_ERR_RDONLY                   (2133571433L)
#define EXT2_ET_DBLIST_EMPTY                     (2133571434L)
#define EXT2_ET_RO_BLOCK_ITERATE                 (2133571435L)
#define EXT2_ET_MAGIC_EXTENT_PATH                (2133571436L)
#define EXT2_ET_MAGIC_GENERIC_BITMAP64           (2133571437L)
#define EXT2_ET_MAGIC_BLOCK_BITMAP64             (2133571438L)
#define EXT2_ET_MAGIC_INODE_BITMAP64             (2133571439L)
#define EXT2_ET_MAGIC_RESERVED_13                (2133571440L)
#define EXT2_ET_MAGIC_RESERVED_14                (2133571441L)
#define EXT2_ET_MAGIC_RESERVED_15                (2133571442L)
#define EXT2_ET_MAGIC_RESERVED_16                (2133571443L)
#define EXT2_ET_MAGIC_RESERVED_17                (2133571444L)
#define EXT2_ET_MAGIC_RESERVED_18                (2133571445L)
#define EXT2_ET_MAGIC_RESERVED_19                (2133571446L)
#define EXT2_ET_EXTENT_HEADER_BAD                (2133571447L)
#define EXT2_ET_EXTENT_INDEX_BAD                 (2133571448L)
#define EXT2_ET_EXTENT_LEAF_BAD                  (2133571449L)
#define EXT2_ET_EXTENT_NO_SPACE                  (2133571450L)
#define EXT2_ET_INODE_NOT_EXTENT                 (2133571451L)
#define EXT2_ET_EXTENT_NO_NEXT                   (2133571452L)
#define EXT2_ET_EXTENT_NO_PREV                   (2133571453L)
#define EXT2_ET_EXTENT_NO_UP                     (2133571454L)
#define EXT2_ET_EXTENT_NO_DOWN                   (2133571455L)
#define EXT2_ET_NO_CURRENT_NODE                  (2133571456L)
#define EXT2_ET_OP_NOT_SUPPORTED                 (2133571457L)
#define EXT2_ET_CANT_INSERT_EXTENT               (2133571458L)
#define EXT2_ET_CANT_SPLIT_EXTENT                (2133571459L)
#define EXT2_ET_EXTENT_NOT_FOUND                 (2133571460L)
#define EXT2_ET_EXTENT_NOT_SUPPORTED             (2133571461L)
#define EXT2_ET_EXTENT_INVALID_LENGTH            (2133571462L)
#define EXT2_ET_IO_CHANNEL_NO_SUPPORT_64         (2133571463L)
#define EXT2_ET_NO_MTAB_FILE                     (2133571464L)
#define EXT2_ET_CANT_USE_LEGACY_BITMAPS          (2133571465L)
#define EXT2_ET_MMP_MAGIC_INVALID                (2133571466L)
#define EXT2_ET_MMP_FAILED                       (2133571467L)
#define EXT2_ET_MMP_FSCK_ON                      (2133571468L)
#define EXT2_ET_MMP_BAD_BLOCK                    (2133571469L)
#define EXT2_ET_MMP_UNKNOWN_SEQ                  (2133571470L)
#define EXT2_ET_MMP_CHANGE_ABORT                 (2133571471L)
#define EXT2_ET_MMP_OPEN_DIRECT                  (2133571472L)
#define EXT2_ET_BAD_DESC_SIZE                    (2133571473L)
#define EXT2_ET_INODE_CSUM_INVALID               (2133571474L)
#define EXT2_ET_INODE_BITMAP_CSUM_INVALID        (2133571475L)
#define EXT2_ET_EXTENT_CSUM_INVALID              (2133571476L)
#define EXT2_ET_DIR_NO_SPACE_FOR_CSUM            (2133571477L)
#define EXT2_ET_DIR_CSUM_INVALID                 (2133571478L)
#define EXT2_ET_EXT_ATTR_CSUM_INVALID            (2133571479L)
#define EXT2_ET_SB_CSUM_INVALID                  (2133571480L)
#define EXT2_ET_UNKNOWN_CSUM                     (2133571481L)
#define EXT2_ET_MMP_CSUM_INVALID                 (2133571482L)
#define EXT2_ET_FILE_EXISTS                      (2133571483L)
#define EXT2_ET_BLOCK_BITMAP_CSUM_INVALID        (2133571484L)
#define EXT2_ET_INLINE_DATA_CANT_ITERATE         (2133571485L)
#define EXT2_ET_EA_BAD_NAME_LEN                  (2133571486L)
#define EXT2_ET_EA_BAD_VALUE_SIZE                (2133571487L)
#define EXT2_ET_BAD_EA_HASH                      (2133571488L)
#define EXT2_ET_BAD_EA_HEADER                    (2133571489L)
#define EXT2_ET_EA_KEY_NOT_FOUND                 (2133571490L)
#define EXT2_ET_EA_NO_SPACE                      (2133571491L)
#define EXT2_ET_MISSING_EA_FEATURE               (2133571492L)
#define EXT2_ET_NO_INLINE_DATA                   (2133571493L)
#define EXT2_ET_INLINE_DATA_NO_BLOCK             (2133571494L)
#define EXT2_ET_INLINE_DATA_NO_SPACE             (2133571495L)
#define EXT2_ET_MAGIC_EA_HANDLE                  (2133571496L)
#define EXT2_ET_INODE_IS_GARBAGE                 (2133571497L)
#define EXT2_ET_EA_BAD_VALUE_OFFSET              (2133571498L)
#define EXT2_ET_JOURNAL_FLAGS_WRONG              (2133571499L)
#define EXT2_ET_UNDO_FILE_CORRUPT                (2133571500L)
#define EXT2_ET_UNDO_FILE_WRONG                  (2133571501L)
#define EXT2_ET_FILESYSTEM_CORRUPTED             (2133571502L)
#define EXT2_ET_BAD_CRC                          (2133571503L)
#define EXT2_ET_CORRUPT_JOURNAL_SB               (2133571504L)
#define EXT2_ET_INODE_CORRUPTED                  (2133571505L)


#define EXT2_FILE_WRITE    0x0001
#define EXT2_FILE_CREATE  0x0002

#define EXT2_ET_DIR_NO_SPACE (2133571366L)

#define EXT2_FT_REG_FILE 1

#define EXT4_INLINE_DATA_FL    0x10000000 /* Inode has inline data */
#define EXT4_EXTENTS_FL     0x00080000 /* Inode uses extents */

#define EXT2_SB(sb)  (sb)
/*
 * Ext2/linux mode flags.  We define them here so that we don't need
 * to depend on the OS's sys/stat.h, since we may be compiling on a
 * non-Linux system.
 */
#define LINUX_S_IFMT  00170000
#define LINUX_S_IFSOCK 0140000
#define LINUX_S_IFLNK   0120000
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

#define LINUX_S_ISLNK(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFLNK)
#define LINUX_S_ISREG(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFREG)
#define LINUX_S_ISDIR(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFDIR)
#define LINUX_S_ISCHR(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFCHR)
#define LINUX_S_ISBLK(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFBLK)
#define LINUX_S_ISFIFO(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFIFO)
#define LINUX_S_ISSOCK(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFSOCK)

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

errcode_t ext2fs_namei_follow(
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
  __u16  i_mode;    /* File mode */
  __u16  i_uid;    /* Low 16 bits of Owner Uid */
  __u32  i_size;    /* Size in bytes */
  __u32  i_atime;  /* Access time */
  __u32  i_ctime;  /* Inode change time */
  __u32  i_mtime;  /* Modification time */
  __u32  i_dtime;  /* Deletion Time */
  __u16  i_gid;    /* Low 16 bits of Group Id */
  __u16  i_links_count;  /* Links count */
  __u32  i_blocks;  /* Blocks count */
  __u32  i_flags;  /* File flags */
  union {
    struct {
      __u32  l_i_version; /* was l_i_reserved1 */
    } linux1;
    struct {
      __u32  h_i_translator;
    } hurd1;
  } osd1;        /* OS dependent 1 */
  __u32  i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
  __u32  i_generation;  /* File version (for NFS) */
  __u32  i_file_acl;  /* File ACL */
  __u32  i_size_high;
  __u32  i_faddr;  /* Fragment address */
  union {
    struct {
      __u16  l_i_blocks_hi;
      __u16  l_i_file_acl_high;
      __u16  l_i_uid_high;  /* these 2 fields    */
      __u16  l_i_gid_high;  /* were reserved2[0] */
      __u16  l_i_checksum_lo; /* crc32c(uuid+inum+inode) */
      __u16  l_i_reserved;
    } linux2;
    struct {
      __u8  h_i_frag;  /* Fragment number */
      __u8  h_i_fsize;  /* Fragment size */
      __u16  h_i_mode_high;
      __u16  h_i_uid_high;
      __u16  h_i_gid_high;
      __u32  h_i_author;
    } hurd2;
  } osd2;        /* OS dependent 2 */
};

struct ext2_inode_large {
  __u16  i_mode;    /* File mode */
  __u16  i_uid;    /* Low 16 bits of Owner Uid */
  __u32  i_size;    /* Size in bytes */
  __u32  i_atime;  /* Access time */
  __u32  i_ctime;  /* Inode Change time */
  __u32  i_mtime;  /* Modification time */
  __u32  i_dtime;  /* Deletion Time */
  __u16  i_gid;    /* Low 16 bits of Group Id */
  __u16  i_links_count;  /* Links count */
  __u32  i_blocks;  /* Blocks count */
  __u32  i_flags;  /* File flags */
  union {
    struct {
      __u32  l_i_version; /* was l_i_reserved1 */
    } linux1;
    struct {
      __u32  h_i_translator;
    } hurd1;
  } osd1;        /* OS dependent 1 */
  __u32  i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
  __u32  i_generation;  /* File version (for NFS) */
  __u32  i_file_acl;  /* File ACL */
  __u32  i_size_high;
  __u32  i_faddr;  /* Fragment address */
  union {
    struct {
      __u16  l_i_blocks_hi;
      __u16  l_i_file_acl_high;
      __u16  l_i_uid_high;  /* these 2 fields    */
      __u16  l_i_gid_high;  /* were reserved2[0] */
      __u16  l_i_checksum_lo; /* crc32c(uuid+inum+inode) */
      __u16  l_i_reserved;
    } linux2;
    struct {
      __u8  h_i_frag;  /* Fragment number */
      __u8  h_i_fsize;  /* Fragment size */
      __u16  h_i_mode_high;
      __u16  h_i_uid_high;
      __u16  h_i_gid_high;
      __u32  h_i_author;
    } hurd2;
  } osd2;        /* OS dependent 2 */
  __u16  i_extra_isize;
  __u16  i_checksum_hi;  /* crc32c(uuid+inum+inode) */
  __u32  i_ctime_extra;  /* extra Change time (nsec << 2 | epoch) */
  __u32  i_mtime_extra;  /* extra Modification time (nsec << 2 | epoch) */
  __u32  i_atime_extra;  /* extra Access time (nsec << 2 | epoch) */
  __u32  i_crtime;  /* File creation time */
  __u32  i_crtime_extra;  /* extra File creation time (nsec << 2 | epoch)*/
  __u32  i_version_hi;  /* high 32 bits for 64-bit version */
  __u32   i_projid;       /* Project ID */
};

/*
 * Constants for ext4's extended time encoding
 */
#define EXT4_EPOCH_BITS 2
#define EXT4_EPOCH_MASK ((1 << EXT4_EPOCH_BITS) - 1)
#define EXT4_NSEC_MASK  (~0UL << EXT4_EPOCH_BITS)

#define EXT2_NAME_LEN 255

struct ext2_dir_entry {
  __u32  inode;      /* Inode number */
  __u16  rec_len;    /* Directory entry length */
  __u16  name_len;    /* Name length */
  char  name[EXT2_NAME_LEN];  /* File name */
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

extern errcode_t ext2fs_dir_iterate2(
	ext2_filsys fs,
	ext2_ino_t dir,
	int flags,
	char *block_buf,
	int (*func)(
		ext2_ino_t  dir,
		int  entry,
		struct ext2_dir_entry *dirent,
		int  offset,
		int  blocksize,
		char  *buf,
		void  *priv_data
	),
	void *priv_data
);

typedef struct ext2_struct_dblist *ext2_dblist;

/* dblist_dir.c */
extern errcode_t ext2fs_dblist_dir_iterate(
	ext2_dblist dblist,
	int  flags,
	char  *block_buf,
	int (*func)(ext2_ino_t  dir,
				int    entry,
				struct ext2_dir_entry *dirent,
				int  offset,
				int  blocksize,
				char  *buf,
				void  *priv_data),
	void *priv_data
);

extern errcode_t ext2fs_file_read(
  ext2_file_t file,
  void *buf,
  unsigned int wanted,
  unsigned int *got
);

extern errcode_t ext2fs_file_flush(ext2_file_t file);

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

errcode_t ext2fs_symlink(
  ext2_filsys fs,
  ext2_ino_t parent,
  ext2_ino_t ino,
  const char *name,
  const char *target
);

errcode_t ext2fs_link(
  ext2_filsys fs,
  ext2_ino_t dir,
  const char *name,
  ext2_ino_t ino,
  int flags
);

errcode_t ext2fs_symlink(
  ext2_filsys fs,
  ext2_ino_t parent,
  ext2_ino_t ino,
  const char *name,
  const char *target
);

int ext2fs_is_fast_symlink(struct ext2_inode *inode);
extern errcode_t ext2fs_get_memzero(unsigned long size, void *ptr);

extern errcode_t ext2fs_inline_data_get(
  ext2_filsys fs,
  ext2_ino_t ino,
  struct ext2_inode *inode,
  void *buf,
  size_t *size
);

extern errcode_t ext2fs_bmap2(
  ext2_filsys fs,
  ext2_ino_t ino,
  struct ext2_inode *inode,
  char *block_buf,
  int bmap_flags,
  blk64_t block,
  int *ret_flags,
  blk64_t *phys_blk
);

errcode_t io_channel_read_blk64(
  io_channel channel,
  unsigned long long block,
  int count,
  void *data
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

extern errcode_t ext2fs_read_inode_full(
  ext2_filsys fs,
  ext2_ino_t ino,
  struct ext2_inode * inode,
  int bufsize
);

extern errcode_t ext2fs_write_inode(
  ext2_filsys fs,
  ext2_ino_t ino,
  struct ext2_inode * inode
);

extern errcode_t ext2fs_write_inode_full(
  ext2_filsys fs,
  ext2_ino_t ino,
  struct ext2_inode * inode,
  int bufsize
);

extern int ext2fs_inode_has_valid_blocks2(
  ext2_filsys fs,
  struct ext2_inode *inode
);

/* punch.c */
/*
 * NOTE: This function removes from an inode the blocks "start", "end", and
 * every block in between.
 */
extern errcode_t ext2fs_punch(
	ext2_filsys fs, ext2_ino_t ino,
	struct ext2_inode *inode,
	char *block_buf, blk64_t start,
	blk64_t end
);

errcode_t ext2fs_free_ext_attr(
  ext2_filsys fs,
  ext2_ino_t ino,
  struct ext2_inode_large *inode
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
  uint32_t  s_inodes_count;    /* Inodes count */
  uint32_t  s_blocks_count;    /* Blocks count */
  uint32_t  s_r_blocks_count;  /* Reserved blocks count */
  uint32_t  s_free_blocks_count;  /* Free blocks count */
  uint32_t  s_free_inodes_count;  /* Free inodes count */
  uint32_t  s_first_data_block;  /* First Data Block */
  uint32_t  s_feature_compat;  /* compatible feature set */
  uint32_t  s_feature_incompat;  /* incompatible feature set */
  uint32_t  s_feature_ro_compat;  /* readonly-compatible feature set */
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
  errcode_t  magic;
  io_manager  manager;
  char    *name;
  int    block_size;
  errcode_t  (*read_error)(
		io_channel channel,
		unsigned long block,
		int count,
		void *data,
		size_t size,
		int actual_bytes_read,
		errcode_t  error
	);
  errcode_t  (*write_error)(
		io_channel channel,
		unsigned long block,
		int count,
		const void *data,
		size_t size,
		int actual_bytes_written,
		errcode_t error
	);
  int    refcount;
  int    flags;
  long    reserved[14];
  void    *private_data;
  void    *app_data;
  int    align;
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
  long  reserved[14];
};

errcode_t ext2fs_open(const char *name, int flags, int superblock, unsigned int block_size, io_manager manager, ext2_filsys *ret_fs);
errcode_t ext2fs_close(ext2_filsys fs);
extern errcode_t ext2fs_flush(ext2_filsys fs);
extern errcode_t ext2fs_flush2(ext2_filsys fs, int flags);
extern errcode_t ext2fs_close_free(ext2_filsys *fs);

errcode_t ext2fs_read_block_bitmap(ext2_filsys fs);

int ext2fs_test_block_bitmap(ext2fs_block_bitmap bitmap, blk_t block);

/*extern errcode_t ext2fs_file_open(ext2_filsys fs, ino_t ino, int flags, ext2_file_t *ret);*/

errcode_t io_channel_discard(io_channel channel, unsigned long long block, unsigned long long count);

/*
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
#define EXT2_FT_UNKNOWN		0
#define EXT2_FT_REG_FILE	1
#define EXT2_FT_DIR		2
#define EXT2_FT_CHRDEV		3
#define EXT2_FT_BLKDEV		4
#define EXT2_FT_FIFO		5
#define EXT2_FT_SOCK		6
#define EXT2_FT_SYMLINK		7

#define EXT2_GOOD_OLD_INODE_SIZE 128

#define EXT2_FEATURE_COMPAT_DIR_PREALLOC  0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES  0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL    0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR    0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INODE  0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX    0x0020
#define EXT2_FEATURE_COMPAT_LAZY_BG    0x0040
/* #define EXT2_FEATURE_COMPAT_EXCLUDE_INODE  0x0080 not used, legacy */
#define EXT2_FEATURE_COMPAT_EXCLUDE_BITMAP  0x0100
#define EXT4_FEATURE_COMPAT_SPARSE_SUPER2  0x0200


#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER  0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE  0x0002
/* #define EXT2_FEATURE_RO_COMPAT_BTREE_DIR  0x0004 not used */
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE  0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM    0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK  0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE  0x0040
#define EXT4_FEATURE_RO_COMPAT_HAS_SNAPSHOT  0x0080
#define EXT4_FEATURE_RO_COMPAT_QUOTA    0x0100
#define EXT4_FEATURE_RO_COMPAT_BIGALLOC    0x0200
/*
 * METADATA_CSUM implies GDT_CSUM.  When METADATA_CSUM is set, group
 * descriptor checksums use the same algorithm as all other data
 * structures' checksums.
 */
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM  0x0400
#define EXT4_FEATURE_RO_COMPAT_REPLICA    0x0800
#define EXT4_FEATURE_RO_COMPAT_READONLY    0x1000
#define EXT4_FEATURE_RO_COMPAT_PROJECT    0x2000 /* Project quota */


#define EXT2_FEATURE_INCOMPAT_COMPRESSION  0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE    0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER    0x0004 /* Needs recovery */
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV  0x0008 /* Journal device */
#define EXT2_FEATURE_INCOMPAT_META_BG    0x0010
#define EXT3_FEATURE_INCOMPAT_EXTENTS    0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT    0x0080
#define EXT4_FEATURE_INCOMPAT_MMP    0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG    0x0200
#define EXT4_FEATURE_INCOMPAT_EA_INODE    0x0400
#define EXT4_FEATURE_INCOMPAT_DIRDATA    0x1000
#define EXT4_FEATURE_INCOMPAT_CSUM_SEED    0x2000
#define EXT4_FEATURE_INCOMPAT_LARGEDIR    0x4000 /* >2GB or 3-lvl htree */
#define EXT4_FEATURE_INCOMPAT_INLINE_DATA  0x8000 /* data in inode */
#define EXT4_FEATURE_INCOMPAT_ENCRYPT    0x10000

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

/*
 * Flags for the ext2_filsys structure and for ext2fs_open()
 */
#define EXT2_FLAG_RW			0x01
#define EXT2_FLAG_CHANGED		0x02
#define EXT2_FLAG_DIRTY			0x04
#define EXT2_FLAG_VALID			0x08
#define EXT2_FLAG_IB_DIRTY		0x10
#define EXT2_FLAG_BB_DIRTY		0x20
#define EXT2_FLAG_SWAP_BYTES		0x40
#define EXT2_FLAG_SWAP_BYTES_READ	0x80
#define EXT2_FLAG_SWAP_BYTES_WRITE	0x100
#define EXT2_FLAG_MASTER_SB_ONLY	0x200
#define EXT2_FLAG_FORCE			0x400
#define EXT2_FLAG_SUPER_ONLY		0x800
#define EXT2_FLAG_JOURNAL_DEV_OK	0x1000
#define EXT2_FLAG_IMAGE_FILE		0x2000
#define EXT2_FLAG_EXCLUSIVE		0x4000
#define EXT2_FLAG_SOFTSUPP_FEATURES	0x8000
#define EXT2_FLAG_NOFREE_ON_ERROR	0x10000
#define EXT2_FLAG_64BITS		0x20000
#define EXT2_FLAG_PRINT_PROGRESS	0x40000
#define EXT2_FLAG_DIRECT_IO		0x80000
#define EXT2_FLAG_SKIP_MMP		0x100000
#define EXT2_FLAG_IGNORE_CSUM_ERRORS	0x200000

/*
 * Mark a filesystem superblock as dirty
 */
inline void ext2fs_mark_super_dirty(ext2_filsys fs) {
	fs->flags |= EXT2_FLAG_DIRTY | EXT2_FLAG_CHANGED;
}

/*
 * Return flags for the directory iterator functions
 */
#define DIRENT_CHANGED	1
#define DIRENT_ABORT	2
#define DIRENT_ERROR	3

EXT4_FEATURE_COMPAT_FUNCS(dir_prealloc,    2, DIR_PREALLOC)
EXT4_FEATURE_COMPAT_FUNCS(imagic_inodes,  2, IMAGIC_INODES)
EXT4_FEATURE_COMPAT_FUNCS(journal,    3, HAS_JOURNAL)
EXT4_FEATURE_COMPAT_FUNCS(xattr,    2, EXT_ATTR)
EXT4_FEATURE_COMPAT_FUNCS(resize_inode,    2, RESIZE_INODE)
EXT4_FEATURE_COMPAT_FUNCS(dir_index,    2, DIR_INDEX)
EXT4_FEATURE_COMPAT_FUNCS(lazy_bg,    2, LAZY_BG)
EXT4_FEATURE_COMPAT_FUNCS(exclude_bitmap,  2, EXCLUDE_BITMAP)
EXT4_FEATURE_COMPAT_FUNCS(sparse_super2,  4, SPARSE_SUPER2)

EXT4_FEATURE_RO_COMPAT_FUNCS(sparse_super,  2, SPARSE_SUPER)
EXT4_FEATURE_RO_COMPAT_FUNCS(large_file,  2, LARGE_FILE)
EXT4_FEATURE_RO_COMPAT_FUNCS(huge_file,    4, HUGE_FILE)
EXT4_FEATURE_RO_COMPAT_FUNCS(gdt_csum,    4, GDT_CSUM)
EXT4_FEATURE_RO_COMPAT_FUNCS(dir_nlink,    4, DIR_NLINK)
EXT4_FEATURE_RO_COMPAT_FUNCS(extra_isize,  4, EXTRA_ISIZE)
EXT4_FEATURE_RO_COMPAT_FUNCS(has_snapshot,  4, HAS_SNAPSHOT)
EXT4_FEATURE_RO_COMPAT_FUNCS(quota,    4, QUOTA)
EXT4_FEATURE_RO_COMPAT_FUNCS(bigalloc,    4, BIGALLOC)
EXT4_FEATURE_RO_COMPAT_FUNCS(metadata_csum,  4, METADATA_CSUM)
EXT4_FEATURE_RO_COMPAT_FUNCS(replica,    4, REPLICA)
EXT4_FEATURE_RO_COMPAT_FUNCS(readonly,    4, READONLY)
EXT4_FEATURE_RO_COMPAT_FUNCS(project,    4, PROJECT)

EXT4_FEATURE_INCOMPAT_FUNCS(compression,  2, COMPRESSION)
EXT4_FEATURE_INCOMPAT_FUNCS(filetype,    2, FILETYPE)
EXT4_FEATURE_INCOMPAT_FUNCS(journal_needs_recovery,  3, RECOVER)
EXT4_FEATURE_INCOMPAT_FUNCS(journal_dev,  3, JOURNAL_DEV)
EXT4_FEATURE_INCOMPAT_FUNCS(meta_bg,    2, META_BG)
EXT4_FEATURE_INCOMPAT_FUNCS(extents,    3, EXTENTS)
EXT4_FEATURE_INCOMPAT_FUNCS(64bit,    4, 64BIT)
EXT4_FEATURE_INCOMPAT_FUNCS(mmp,    4, MMP)
EXT4_FEATURE_INCOMPAT_FUNCS(flex_bg,    4, FLEX_BG)
EXT4_FEATURE_INCOMPAT_FUNCS(ea_inode,    4, EA_INODE)
EXT4_FEATURE_INCOMPAT_FUNCS(dirdata,    4, DIRDATA)
EXT4_FEATURE_INCOMPAT_FUNCS(csum_seed,    4, CSUM_SEED)
EXT4_FEATURE_INCOMPAT_FUNCS(largedir,    4, LARGEDIR)
EXT4_FEATURE_INCOMPAT_FUNCS(inline_data,  4, INLINE_DATA)
EXT4_FEATURE_INCOMPAT_FUNCS(encrypt,    4, ENCRYPT)

static void increment_version(struct ext2_inode *inode) {
  inode->osd1.linux1.l_i_version++;
}

#ifdef __cplusplus
}
#endif

#define __EXT2FS_H
#endif /* !defined(__EXT2FS_H) */
