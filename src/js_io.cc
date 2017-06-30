#include "js_io.h"

typedef void disk_request_t(
	unsigned short type,
	unsigned long offset,
	unsigned long length,
	char* data,
	int* lock
);

typedef void mount_callback_t(errcode_t, ext2_filsys);

typedef void trim_callback_t(errcode_t);

errcode_t js_request_entry(
	io_channel channel,
	int type,
	unsigned long long block,
	int count,
	void *data
) {
	disk_request_t* disk_request = reinterpret_cast<disk_request_t*>(
		channel->private_data
	);
	int lock = 0;  // 0: locked; 1: unlocked

	unsigned int size = (count < 0) ? -count : (count * channel->block_size);
	unsigned int offset = block * channel->block_size;

	disk_request(type, offset, size, static_cast<char*>(data), &lock);

	// Poor man's lock
	while(lock == 0) {
		emscripten_sleep(10);
	}

	return 0;
}

static errcode_t js_open_entry(
	const char *hex_ptr,
	int flags,
	io_channel *channel
) {
	io_channel io = (io_channel) malloc(sizeof(struct struct_io_channel));
	memset(io, 0, sizeof(struct struct_io_channel));
	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	io->manager = get_js_io_manager();
	sscanf(hex_ptr, "%p", &io->private_data);
	*channel = io;
	return js_request_entry(io, 0, 0, 0, NULL);
}

static errcode_t js_close_entry(io_channel channel) {
	return js_request_entry(channel, 1, 0, 0, NULL);
}

static errcode_t set_blksize(io_channel channel, int blksize) {
	channel->block_size = blksize;
	return 0;
}

static errcode_t js_read_blk_entry(
	io_channel channel,
	unsigned long block,
	int count,
	void *data
) {
	return js_request_entry(channel, 2, block, count, data);
}

static errcode_t js_write_blk_entry(
	io_channel channel,
	unsigned long block,
	int count,
	const void *data
) {
	return js_request_entry(channel, 3, block, count, (void*)data);
}

static errcode_t js_flush_entry(io_channel channel) {
	return js_request_entry(channel, 4, 0, 0, NULL);
}

static errcode_t js_read_blk64_entry(
	io_channel channel,
	unsigned long long block,
	int count,
	void *data
) {
	return js_request_entry(channel, 2, block, count, data);
}

static errcode_t js_write_blk64_entry(
	io_channel channel,
	unsigned long long block,
	int count,
	const void *data
) {
	return js_request_entry(channel, 3, block, count, (void*)data);
}

static errcode_t js_discard_entry(
	io_channel channel,
	unsigned long long block,
	unsigned long long count
) {
	return js_request_entry(channel, 10, block, count, NULL);
}

static errcode_t js_cache_readahead_entry(
	io_channel channel,
	unsigned long long block,
	unsigned long long count
) {
	return js_request_entry(channel, 11, block, count, NULL);
}

static errcode_t js_zeroout_entry(
	io_channel channel,
	unsigned long long block,
	unsigned long long count
) {
	return js_request_entry(channel, 12, block, count, NULL);
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

void mount(disk_request_t* disk_request, mount_callback_t* callback) {
	char hex_ptr[sizeof(void*) * 2 + 3];
	sprintf(hex_ptr, "%p", disk_request);
	ext2_filsys fs;
	io_manager man = get_js_io_manager();
	int ret = ext2fs_open(hex_ptr, EXT2_FLAG_RW, 0, 0, man, &fs);
	callback(ret, fs);
}

void trim(unsigned int fsPointer, trim_callback_t* callback) {
	ext2_filsys fs = reinterpret_cast<ext2_filsys>(fsPointer);

	unsigned int start, blk, count;
	errcode_t ret;

	if (!fs->block_map) {
		if ((ret = ext2fs_read_block_bitmap(fs))) {
			callback(ret);
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
	callback(0);
}

emscripten::val getBytes(unsigned int address, unsigned int length) {
	// TODO: check if this doesn't exist in emscripten
	unsigned char* p  = reinterpret_cast<unsigned char*>(address);
	return emscripten::val(emscripten::typed_memory_view(length, p));
}

EMSCRIPTEN_BINDINGS() {
	function("getBytes", &getBytes, emscripten::allow_raw_pointers());
}
