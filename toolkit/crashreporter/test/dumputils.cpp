#include "google_breakpad/processor/minidump.h"
#include "nscore.h"

using namespace google_breakpad;

extern "C"
NS_EXPORT bool
DumpHasStream(const char* dump_file, u_int32_t stream_type)
{
  Minidump dump(dump_file);
  if (!dump.Read())
    return false;

  u_int32_t length;
  if (!dump.SeekToStreamType(stream_type, &length) || length == 0)
    return false;

  return true;
}
