#include <stdio.h>
#include <nan.h>

#include "ext2fs.h"
#include "async.h"

#include "js_io.h"

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

static void js_request_done(v8::Local<v8::Value> ret, void* _s) {
	auto *s = reinterpret_cast<request_state_t*>(_s);
	if (ret->IsNull()) {
		s->ret = 0;
	} else {
		s->ret = static_cast<errcode_t>(Nan::To<int64_t>(ret).FromJust());
	}

	uv_sem_post(&s->js_sem);
}

static void noop(char *data, void *hint) {}

static void js_request(void *_s) {
	HandleScope scope;

	v8::Local<v8::Value> buffer;
	unsigned long long offset, size;
	auto s = reinterpret_cast<request_state_t*>(_s);

	auto request_cb = static_cast<Callback*>(s->channel->private_data);

	offset = static_cast<unsigned long long>(s->block * s->channel->block_size);
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
		New<v8::External>(reinterpret_cast<void *>(js_request_done)),
		New<v8::External>(s),
		get_callback(),
	};

	request_cb->Call(7, argv);
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

	errcode_t ret = ext2fs_get_mem(sizeof(struct struct_io_channel), &io);
	if (ret) {
		return ret;
	}
	memset(io, 0, sizeof(struct struct_io_channel));

	io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
	io->manager = get_js_io_manager();
	sscanf(hex_ptr, "%p", &io->private_data);

	*channel = io;
	return js_request_entry(io, 0, 0, 0, NULL);
}

static errcode_t js_close_entry(io_channel channel) {
	errcode_t ret = js_request_entry(channel, 1, 0, 0, NULL);
	if (ret) {
		return ret;
	}
	ext2fs_free_mem(&channel);
	return ret;
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
	return static_cast<errcode_t>(js_request_entry(channel, 10, block, static_cast<int>(count), NULL));  // TODO: what if count doesn't fit in int?
}

static errcode_t js_cache_readahead_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	return static_cast<errcode_t>(js_request_entry(channel, 11, block, static_cast<int>(count), NULL));
}

static errcode_t js_zeroout_entry(io_channel channel, unsigned long long block, unsigned long long count) {
	return static_cast<errcode_t>(js_request_entry(channel, 12, block, static_cast<int>(count), NULL));
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
