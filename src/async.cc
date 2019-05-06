#include "async.h"

using namespace Nan;

uv_async_t *async = NULL;
uv_mutex_t lock;

static void default_loop_entry(uv_async_t* handle) {
	call_info_t *info = (call_info_t*) handle->data;

	info->fn(info->args);
	// Unlock run_on_default_loop
	uv_sem_post(&info->sem);
}

static NAN_METHOD(callback_wrapper) {
	if ((async == NULL) || (uv_is_closing((uv_handle_t*)async) != 0)) {
		return;
	}
	void (*fn)(v8::Local<v8::Value>, void *) = reinterpret_cast<void (*)(v8::Local<v8::Value>, void *)>(info[1].As<v8::External>()->Value());
	void *s = static_cast<void *>(info[2].As<v8::External>()->Value());
	fn(info[0], s);
}

static bool persistent_callback_initialized = false;

void init_async() {
	async = new uv_async_t;
	uv_async_init(uv_default_loop(), async, default_loop_entry);
	uv_mutex_init(&lock);
	if (!persistent_callback_initialized) {
		persistent_callback.Reset(Nan::GetFunction(Nan::New<v8::FunctionTemplate>(callback_wrapper)).ToLocalChecked());
		persistent_callback_initialized = true;
	}
}

void close_async() {
	uv_mutex_destroy(&lock);
	uv_close((uv_handle_t*)async, [](uv_handle_t* handle) {
		delete handle;
	});
}

void run_on_default_loop(void (*fn)(void *), void *args) {
	// This function needs to be mutex'd because uv_async_send can coalesce
	// calls if called multiple times. This could potentially be avoided with a
	// heap allocated queue that stores requests for the main thread but the
	// benefit would be small.
	uv_mutex_lock(&lock);

	call_info_t info;
	info.fn = fn;
	info.args = args;
	uv_sem_init(&info.sem, 0);

	async->data = &info;
	uv_async_send(async);

	uv_sem_wait(&info.sem);
	uv_mutex_unlock(&lock);
}

v8::Local<v8::Function> get_callback() {
	return New(persistent_callback);
}
