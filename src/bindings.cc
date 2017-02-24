#include "node_ext2fs.h"

NAN_MODULE_INIT(InitAll) {
	NAN_EXPORT(target, mount);
}

NODE_MODULE(bindings, InitAll)
