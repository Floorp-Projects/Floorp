#include <stdio.h>

#include "google_breakpad/processor/minidump.h"
#include "nscore.h"

using namespace google_breakpad;

// Return true if the specified minidump contains a stream of |stream_type|.
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

// Return true if the specified minidump contains a memory region
// that contains the instruction pointer from the exception record.
extern "C"
NS_EXPORT bool
DumpHasInstructionPointerMemory(const char* dump_file)
{
  Minidump minidump(dump_file);
  if (!minidump.Read())
    return false;

  MinidumpException* exception = minidump.GetException();
  MinidumpMemoryList* memory_list = minidump.GetMemoryList();
  if (!exception || !memory_list) {
    return false;
  }

  MinidumpContext* context = exception->GetContext();
  if (!context)
    return false;

  u_int64_t instruction_pointer;
  switch (context->GetContextCPU()) {
  case MD_CONTEXT_X86:
    instruction_pointer = context->GetContextX86()->eip;
    break;
  case MD_CONTEXT_AMD64:
    instruction_pointer = context->GetContextAMD64()->rip;
    break;
  case MD_CONTEXT_ARM:
    instruction_pointer = context->GetContextARM()->iregs[15];
    break;
  default:
    return false;
  }

  MinidumpMemoryRegion* region =
    memory_list->GetMemoryRegionForAddress(instruction_pointer);
  return region != NULL;
}

// This function tests for a very specific condition. It finds
// an address in a file, "crash-addr", in the CWD. It checks
// that the minidump has a memory region starting at that
// address. The region must be 32 bytes long and contain the
// values 0 to 31 as bytes, in ascending order.
extern "C"
NS_EXPORT bool
DumpCheckMemory(const char* dump_file)
{
  Minidump dump(dump_file);
  if (!dump.Read())
    return false;

  MinidumpMemoryList* memory_list = dump.GetMemoryList();
  if (!memory_list) {
    return false;
  }

  void *addr;
  FILE *fp = fopen("crash-addr", "r");
  if (!fp)
    return false;
  if (fscanf(fp, "%p", &addr) != 1)
    return false;
  fclose(fp);

  remove("crash-addr");

  MinidumpMemoryRegion* region =
    memory_list->GetMemoryRegionForAddress(u_int64_t(addr));
  if(!region)
    return false;

  const u_int8_t* chars = region->GetMemory();
  if (region->GetSize() != 32)
    return false;

  for (int i=0; i<32; i++) {
    if (chars[i] != i)
      return false;
  }

  return true;
}
