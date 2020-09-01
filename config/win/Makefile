V =
ifeq ($(strip $(V)),)
    E = @echo
    Q = @
else
    E = @echo
    Q =
endif

ARCH:=$(or $(npm_config_target_arch),$(shell node -e "console.log(os.arch())"))
ifeq ($(ARCH),ia32)
CC = i686-w64-mingw32-gcc
AR = i686-w64-mingw32-gcc-ar
else ifeq ($(ARCH),x64)
CC = x86_64-w64-mingw32-gcc
AR = x86_64-w64-mingw32-gcc-ar
else
CC = gcc
AR = ar
endif

srcdir=../..
top_srcdir=../../../..

CFLAGS = -g -O3 -fPIC -DHAVE_CONFIG_H \
	   -I$(srcdir)/lib \
	   -I$(top_srcdir)/config/common \
	   -I$(top_srcdir)/config/win/ia32

OBJS= alloc.o \
	alloc_sb.o \
	alloc_stats.o \
	alloc_tables.o \
	atexit.o \
	badblocks.o \
	bb_inode.o \
	bitmaps.o \
	bitops.o \
	blkmap64_ba.o \
	blkmap64_rb.o \
	blknum.o \
	block.o \
	bmap.o \
	check_desc.o \
	closefs.o \
	crc16.o \
	crc32c.o \
	csum.o \
	dblist.o \
	dblist_dir.o \
	dir_iterate.o \
	dirblock.o \
	dirhash.o \
	expanddir.o \
	ext_attr.o \
	extent.o \
	fallocate.o \
	fileio.o \
	finddev.o \
	flushb.o \
	freefs.o \
	gen_bitmap.o \
	gen_bitmap64.o \
	get_num_dirs.o \
	get_pathname.o \
	getsectsize.o \
	getsize.o \
	i_block.o \
	icount.o \
	ind_block.o \
	initialize.o \
	inline.o \
	inline_data.o \
	inode.o \
	io_manager.o \
	ismounted.o \
	link.o \
	llseek.o \
	lookup.o \
	mkdir.o \
	mkjournal.o \
	mmp.o \
	namei.o \
	native.o \
	newdir.o \
	openfs.o \
	progress.o \
	punch.o \
	rbtree.o \
	read_bb.o \
	read_bb_file.o \
	res_gdt.o \
	rw_bitmaps.o \
	sha512.o \
	swapfs.o \
	symlink.o \
	unlink.o \
	valid_blk.o \
	version.o

all: directory libext2fs.a

directory:
	mkdir -p $(ARCH)

# We need these objects from libgcc for windows 32bit builds
$(ARCH)/_umoddi3.o:
	$(E) "	EXTRACT_OBJ $@"
	$(Q) $(AR) x  "$(shell $(CC) --print-file-name=libgcc.a)" _umoddi3.o
	mv _umoddi3.o $(ARCH)/_umoddi3.o

$(ARCH)/_udivdi3.o:
	$(E) "	EXTRACT_OBJ $@"
	$(Q) $(AR) x "$(shell $(CC) --print-file-name=libgcc.a)" _udivdi3.o
	mv _udivdi3.o $(ARCH)/_udivdi3.o

libext2fs.a: $(addprefix $(ARCH)/,$(OBJS) _umoddi3.o _udivdi3.o)
	$(E) "	GEN_LIB $@"
	$(Q) $(AR) rc $@ $(addprefix $(ARCH)/,$(OBJS) _udivdi3.o _umoddi3.o)

$(ARCH)/%.o: %.c
	$(E) "	CC $<"
	$(Q) $(CC) $(CFLAGS) -c $< -o $@
