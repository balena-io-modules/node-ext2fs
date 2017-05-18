#include "node_ext2fs.h"

NAN_MODULE_INIT(InitAll) {
	NAN_EXPORT(target, trim);
	NAN_EXPORT(target, mount);
	NAN_EXPORT(target, umount);
	NAN_EXPORT(target, init);
	NAN_EXPORT(target, close);
}

NODE_MODULE(bindings, InitAll)
