extern "C" {
	#include <sys/types.h>
	#include <ext2fs/ext2fs.h>
}

#include <stdio.h>
#include <nan.h>
#include "js_io.h"
#include "async.h"

using namespace Nan;

struct request_state_t {
	errcode_t ret;
	io_channel channel;
	int type;
	unsigned long long block;
	int count;
	void *data;
	uv_sem_t js_sem;
};

static void js_request_done(NAN_METHOD_ARGS_TYPE info, void* _s) {
	auto *s = reinterpret_cast<request_state_t*>(_s);
	if (info[0]->IsNull()) {
		s->ret = 0;
	} else {
		s->ret = info[0]->IntegerValue();
	}

	uv_sem_post(&s->js_sem);
}

static void noop(char *data, void *hint) {}

static void js_request(void *_s) {
	HandleScope scope;

	v8::Local<v8::Value> buffer;
	unsigned int offset, size;
	auto s = reinterpret_cast<request_state_t*>(_s);

	auto request_cb = static_cast<Callback*>(s->channel->private_data);

	offset = s->block * s->channel->block_size;
	size = (s->count < 0) ? -s->count : (s->count * s->channel->block_size);

	if (s->data) {
		// Convert data pointer to a Buffer object pointing to the same memory.
		// We pass `noop` as the free callback because memory is managed by
		// libe2fs and we don't want V8 to run free() on it.
		buffer = NewBuffer((char*) s->data, size, noop, NULL).ToLocalChecked();
	} else {
		buffer = Null();
	}

	v8::Local<v8::Value> argv[] = {
		New<v8::Number>(s->type),
		New<v8::Number>(offset),
		New<v8::Number>(size),
		buffer,
		make_callback(js_request_done, s),
	};

	request_cb->Call(5, argv);
}

errcode_t js_request_entry(io_channel channel, int type, unsigned long long block, int count, void *data) {
	request_state_t s;
	s.channel = channel;
	s.type = type;
	s.block = block;
	s.count = count;
	s.data = data;
	uv_sem_init(&s.js_sem, 0);

	run_on_default_loop(js_request, &s);

	uv_sem_wait(&s.js_sem);
	uv_sem_destroy(&s.js_sem);
	return s.ret;
}

static errcode_t js_open_entry(const char *hex_ptr, int flags, io_channel *channel) {
	io_channel io = NULL;

	io = (io_channel) malloc(sizeof(struct struct_io_channel));
	memset(io, 0, sizeof(struct struct_io_channel));

	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	io->manager = &js_io_manager;
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

static errcode_t js_read_blk_entry(io_channel channel, unsigned long block, int count, void *data) {
	return js_request_entry(channel, 2, block, count, data);
}

static errcode_t js_write_blk_entry(io_channel channel, unsigned long block, int count, const void *data) {
	return js_request_entry(channel, 3, block, count, const_cast<void *>(data));
}

static errcode_t js_flush_entry(io_channel channel) {
	return js_request_entry(channel, 4, 0, 0, NULL);
}

static errcode_t js_read_blk64_entry(io_channel channel, unsigned long long block, int count, void *data) {
	return js_request_entry(channel, 2, block, count, data);
}

static errcode_t js_write_blk64_entry(io_channel channel, unsigned long long block, int count, const void *data) {
	return js_request_entry(channel, 3, block, count, const_cast<void*>(data));
}

static errcode_t js_discard_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	return js_request_entry(channel, 10, block, count, NULL);
}

static errcode_t js_cache_readahead_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	return js_request_entry(channel, 11, block, count, NULL);
}

static errcode_t js_zeroout_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	return js_request_entry(channel, 12, block, count, NULL);
}

struct struct_io_manager js_io_manager = {
	.magic            =  EXT2_ET_MAGIC_IO_MANAGER,
	.name             =  "JavaScript IO Manager",
	.open             =  js_open_entry,
	.close            =  js_close_entry,
	.set_blksize      =  set_blksize,
	.read_blk         =  js_read_blk_entry,
	.write_blk        =  js_write_blk_entry,
	.flush            =  js_flush_entry,
	.write_byte       =  NULL,
	.set_option       =  NULL,
	.get_stats        =  NULL,
	.read_blk64       =  js_read_blk64_entry,
	.write_blk64      =  js_write_blk64_entry,
	.discard          =  js_discard_entry,
	.cache_readahead  =  js_cache_readahead_entry,
	.zeroout          =  js_zeroout_entry,
};
