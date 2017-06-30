#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "ext2fs.h"

io_manager get_js_io_manager();
