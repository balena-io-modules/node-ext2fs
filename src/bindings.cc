#include "node_ext2fs.h"

NAN_MODULE_INIT(InitAll) {
	NAN_EXPORT(target, trim);
	NAN_EXPORT(target, mount);
	NAN_EXPORT(target, umount);
	NAN_EXPORT(target, open);
	NAN_EXPORT(target, init);
	NAN_EXPORT(target, closeExt);
	NAN_EXPORT(target, close);
	NAN_EXPORT(target, read);
	NAN_EXPORT(target, fstat_);
	NAN_EXPORT(target, readdir);
	NAN_EXPORT(target, write);
	NAN_EXPORT(target, unlink_);
	NAN_EXPORT(target, rmdir);
	NAN_EXPORT(target, mkdir);
	NAN_EXPORT(target, fchmod);
	NAN_EXPORT(target, fchown);
}

NODE_MODULE(bindings, InitAll)
