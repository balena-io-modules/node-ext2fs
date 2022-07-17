io_manager get_js_io_manager();
static int unlink_file_by_name(ext2_filsys fs, const char *path);
static int update_ctime(ext2_filsys fs, ext2_ino_t ino, struct ext2_inode_large *pinode);
static int update_mtime(ext2_filsys fs, ext2_ino_t ino, struct ext2_inode_large *pinode);
static int ext2_file_type(unsigned int mode);
errcode_t node_ext2fs_rmdir(ext2_filsys fs, const char *path);
errcode_t node_ext2fs_rename(ext2_filsys fs, const char *from, const char *to);
errcode_t node_ext2fs_unlink(ext2_filsys fs, const char *path);
errcode_t node_ext2fs_link(ext2_filsys fs, const char *src, const char *dest);
static int rmdir_proc(
  ext2_ino_t dir,
  int	entry,
  struct ext2_dir_entry *dirent,
  int	offset,
  int	blocksize,
  char	*buf,
  void	*private
);