V =
ifeq ($(strip $(V)),)
    E = @echo
    Q = @
else
    E = @echo
    Q =
endif

srcdir=deps/e2fsprogs/
libext2fsdir=$(srcdir)lib/ext2fs/
prejs=src/pre.js
glue=src/glue

CC = emcc
CFLAGS = -DHAVE_CONFIG_H \
	   -I$(srcdir)/lib \
	   -Iconfig/common \
	   -Iconfig/emscripten \
	   -O3

JSFLAGS = \
	-s ASYNCIFY \
	-s ASYNCIFY_IMPORTS="['blk_read', 'blk_write', 'discard', 'flush']" \
	-s EXPORTED_FUNCTIONS="['_malloc_from_js', '_free_from_js', '_node_ext2fs_mount', '_node_ext2fs_trim', '_node_ext2fs_readdir', '_node_ext2fs_open', '_node_ext2fs_read', '_node_ext2fs_write', '_node_ext2fs_unlink', '_node_ext2fs_chmod', '_node_ext2fs_chown', '_node_ext2fs_mkdir', '_node_ext2fs_readlink', '_node_ext2fs_symlink', '_node_ext2fs_close', '_node_ext2fs_umount', '_node_ext2fs_stat_i_mode', '_node_ext2fs_stat_i_links_count', '_node_ext2fs_stat_i_uid', '_node_ext2fs_stat_i_gid', '_node_ext2fs_stat_blocksize', '_node_ext2fs_stat_ino', '_node_ext2fs_stat_i_size', '_node_ext2fs_stat_i_blocks', '_node_ext2fs_stat_i_atime', '_node_ext2fs_stat_i_mtime', '_node_ext2fs_stat_i_ctime']" \
	-s EXPORTED_RUNTIME_METHODS="['ccall']" \
	--pre-js $(prejs)

OBJS= \
	$(libext2fsdir)alloc.o \
	$(libext2fsdir)alloc_sb.o \
	$(libext2fsdir)alloc_stats.o \
	$(libext2fsdir)alloc_tables.o \
	$(libext2fsdir)atexit.o \
	$(libext2fsdir)badblocks.o \
	$(libext2fsdir)bb_inode.o \
	$(libext2fsdir)bitmaps.o \
	$(libext2fsdir)bitops.o \
	$(libext2fsdir)blkmap64_ba.o \
	$(libext2fsdir)blkmap64_rb.o \
	$(libext2fsdir)blknum.o \
	$(libext2fsdir)block.o \
	$(libext2fsdir)bmap.o \
	$(libext2fsdir)check_desc.o \
	$(libext2fsdir)closefs.o \
	$(libext2fsdir)crc16.o \
	$(libext2fsdir)crc32c.o \
	$(libext2fsdir)csum.o \
	$(libext2fsdir)dblist.o \
	$(libext2fsdir)dblist_dir.o \
	$(libext2fsdir)dir_iterate.o \
	$(libext2fsdir)dirblock.o \
	$(libext2fsdir)dirhash.o \
	$(libext2fsdir)expanddir.o \
	$(libext2fsdir)ext_attr.o \
	$(libext2fsdir)extent.o \
	$(libext2fsdir)fallocate.o \
	$(libext2fsdir)fileio.o \
	$(libext2fsdir)finddev.o \
	$(libext2fsdir)flushb.o \
	$(libext2fsdir)freefs.o \
	$(libext2fsdir)gen_bitmap.o \
	$(libext2fsdir)gen_bitmap64.o \
	$(libext2fsdir)get_num_dirs.o \
	$(libext2fsdir)get_pathname.o \
	$(libext2fsdir)getsectsize.o \
	$(libext2fsdir)getsize.o \
	$(libext2fsdir)i_block.o \
	$(libext2fsdir)icount.o \
	$(libext2fsdir)ind_block.o \
	$(libext2fsdir)initialize.o \
	$(libext2fsdir)inline.o \
	$(libext2fsdir)inline_data.o \
	$(libext2fsdir)inode.o \
	$(libext2fsdir)io_manager.o \
	$(libext2fsdir)ismounted.o \
	$(libext2fsdir)link.o \
	$(libext2fsdir)llseek.o \
	$(libext2fsdir)lookup.o \
	$(libext2fsdir)mkdir.o \
	$(libext2fsdir)mkjournal.o \
	$(libext2fsdir)mmp.o \
	$(libext2fsdir)namei.o \
	$(libext2fsdir)native.o \
	$(libext2fsdir)newdir.o \
	$(libext2fsdir)openfs.o \
	$(libext2fsdir)progress.o \
	$(libext2fsdir)punch.o \
	$(libext2fsdir)rbtree.o \
	$(libext2fsdir)read_bb.o \
	$(libext2fsdir)read_bb_file.o \
	$(libext2fsdir)res_gdt.o \
	$(libext2fsdir)rw_bitmaps.o \
	$(libext2fsdir)sha512.o \
	$(libext2fsdir)swapfs.o \
	$(libext2fsdir)symlink.o \
	$(libext2fsdir)unlink.o \
	$(libext2fsdir)valid_blk.o \
	$(libext2fsdir)version.o \
	$(libext2fsdir)../et/error_message.o \
	$(libext2fsdir)../et/et_name.o \
	$(libext2fsdir)../et/init_et.o \
	$(libext2fsdir)../et/com_err.o \
	$(libext2fsdir)../et/com_right.o

all: lib/libext2fs.js

%.o: %.c
	$(E) "	CC $<"
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

$(glue).o: $(glue).c $(glue).h
	$(E) "	CC $<"
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

lib/libext2fs.js: $(OBJS) $(glue).o $(prejs)
	$(E) "	JSGEN $@"
	$(Q) $(CC) $(CFLAGS) $(JSFLAGS) $(OBJS) $(glue).o -o $@
	npx prettier --write ./lib/libext2fs.js

clean:
	rm -f $(OBJS) $(glue).o lib/libext2fs.js lib/libext2fs.wasm
