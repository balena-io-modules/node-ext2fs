#include <nan.h>

using namespace Nan;

static Persistent<v8::Function> persistent_callback;

struct call_info_t {
	void (*fn)(void *);
	void *args;
	uv_sem_t sem;
};

void init_async();

void close_async();

void run_on_default_loop(void (*fn)(void *), void *args);

v8::Local<v8::Function> get_callback();
