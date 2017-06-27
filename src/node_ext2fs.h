#include <nan.h>

NAN_METHOD(trim);
NAN_METHOD(mount);
NAN_METHOD(umount);
NAN_METHOD(open);
NAN_METHOD(init);
NAN_METHOD(closeExt);
NAN_METHOD(close);
NAN_METHOD(read);
NAN_METHOD(fstat_);  // If this is called "fstat", the module doesn't compile on windows.
NAN_METHOD(readdir);
NAN_METHOD(write);
NAN_METHOD(unlink_);  // If this is called "unlink", the module doesn't compile on windows.
NAN_METHOD(rmdir);
NAN_METHOD(mkdir);
NAN_METHOD(fchmod);
NAN_METHOD(fchown);
