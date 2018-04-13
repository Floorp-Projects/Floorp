#include "mozilla/Assertions.h"

#include <stdio.h>
#include <map>

#include "nscore.h"
#include "mozilla/Unused.h"
#include "ExceptionThrower.h"

#ifdef XP_WIN
#include <malloc.h>
#include <windows.h>
#endif

/*
 * This pure virtual call example is from MSDN
 */
class A;

void fcn( A* );

class A
{
public:
  virtual void f() = 0;
  A() { fcn( this ); }
};

class B : A
{
  void f() override { }
public:
  void use() { }
};

void fcn( A* p )
{
  p->f();
}

void PureVirtualCall()
{
  // generates a pure virtual function call
  B b;
  b.use(); // make sure b's actually used
}

extern "C" {
#if XP_WIN && HAVE_64BIT_BUILD && !defined(__MINGW32__)
  // Implementation in win64unwindInfoTests.asm
  uint64_t x64CrashCFITest_NO_MANS_LAND(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_Launcher(uint64_t returnpfn, void* testProc);
  uint64_t x64CrashCFITest_UnknownOpcode(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_PUSH_NONVOL(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_ALLOC_SMALL(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_ALLOC_LARGE(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_SAVE_NONVOL(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_SAVE_NONVOL_FAR(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_SAVE_XMM128(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_SAVE_XMM128_FAR(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_EPILOG(uint64_t returnpfn, void*);
  uint64_t x64CrashCFITest_EOF(uint64_t returnpfn, void*);
#endif // XP_WIN && HAVE_64BIT_BUILD && !defined(__MINGW32__)
}

// Keep these in sync with CrashTestUtils.jsm!
const int16_t CRASH_INVALID_POINTER_DEREF = 0;
const int16_t CRASH_PURE_VIRTUAL_CALL     = 1;
const int16_t CRASH_OOM                   = 3;
const int16_t CRASH_MOZ_CRASH             = 4;
const int16_t CRASH_ABORT                 = 5;
const int16_t CRASH_UNCAUGHT_EXCEPTION    = 6;
const int16_t CRASH_X64CFI_NO_MANS_LAND   = 7;
const int16_t CRASH_X64CFI_LAUNCHER       = 8;
const int16_t CRASH_X64CFI_UNKNOWN_OPCODE = 9;
const int16_t CRASH_X64CFI_PUSH_NONVOL    = 10;
const int16_t CRASH_X64CFI_ALLOC_SMALL    = 11;
const int16_t CRASH_X64CFI_ALLOC_LARGE    = 12;
const int16_t CRASH_X64CFI_SAVE_NONVOL    = 15;
const int16_t CRASH_X64CFI_SAVE_NONVOL_FAR = 16;
const int16_t CRASH_X64CFI_SAVE_XMM128    = 17;
const int16_t CRASH_X64CFI_SAVE_XMM128_FAR = 18;
const int16_t CRASH_X64CFI_EPILOG         = 19;
const int16_t CRASH_X64CFI_EOF            = 20;

#if XP_WIN && HAVE_64BIT_BUILD && !defined(__MINGW32__)

typedef decltype(&x64CrashCFITest_UnknownOpcode) win64CFITestFnPtr_t;

static std::map<int16_t, win64CFITestFnPtr_t>
GetWin64CFITestMap() {
  std::map<int16_t, win64CFITestFnPtr_t> ret = {
    { CRASH_X64CFI_NO_MANS_LAND, x64CrashCFITest_NO_MANS_LAND},
    { CRASH_X64CFI_LAUNCHER, x64CrashCFITest_Launcher},
    { CRASH_X64CFI_UNKNOWN_OPCODE, x64CrashCFITest_UnknownOpcode},
    { CRASH_X64CFI_PUSH_NONVOL, x64CrashCFITest_PUSH_NONVOL},
    { CRASH_X64CFI_ALLOC_SMALL, x64CrashCFITest_ALLOC_SMALL },
    { CRASH_X64CFI_ALLOC_LARGE, x64CrashCFITest_ALLOC_LARGE },
    { CRASH_X64CFI_SAVE_NONVOL, x64CrashCFITest_SAVE_NONVOL },
    { CRASH_X64CFI_SAVE_NONVOL_FAR, x64CrashCFITest_SAVE_NONVOL_FAR },
    { CRASH_X64CFI_SAVE_XMM128, x64CrashCFITest_SAVE_XMM128 },
    { CRASH_X64CFI_SAVE_XMM128_FAR, x64CrashCFITest_SAVE_XMM128_FAR },
    { CRASH_X64CFI_EPILOG, x64CrashCFITest_EPILOG },
    { CRASH_X64CFI_EOF, x64CrashCFITest_EOF }
  };
  // ret values point to jump table entries, not the actual function bodies.
  // Get the correct pointer by calling the function with returnpfn=1
  for (auto it = ret.begin(); it != ret.end(); ++ it) {
    it->second = (win64CFITestFnPtr_t)it->second(1, nullptr);
  }
  return ret;
}

// This ensures tests have enough committed stack space.
// Must not be inlined, or the stack space would not be freed for the caller
// to use.
void MOZ_NEVER_INLINE ReserveStack() {
  // We must actually use the memory in some way that the compiler can't
  // optimize away.
  static const size_t elements = (1024000 / sizeof(FILETIME)) + 1;
  FILETIME stackmem[elements];
  ::GetSystemTimeAsFileTime(&stackmem[0]);
  ::GetSystemTimeAsFileTime(&stackmem[elements - 1]);
 }


#endif // XP_WIN && HAVE_64BIT_BUILD

extern "C" NS_EXPORT
void Crash(int16_t how)
{
  switch (how) {
  case CRASH_INVALID_POINTER_DEREF: {
    volatile int* foo = (int*)0x42;
    *foo = 0;
    // not reached
    break;
  }
  case CRASH_PURE_VIRTUAL_CALL: {
    PureVirtualCall();
    // not reached
    break;
  }
  case CRASH_OOM: {
    mozilla::Unused << moz_xmalloc((size_t) -1);
    mozilla::Unused << moz_xmalloc((size_t) -1);
    mozilla::Unused << moz_xmalloc((size_t) -1);
    break;
  }
  case CRASH_MOZ_CRASH: {
    MOZ_CRASH();
    break;
  }
  case CRASH_ABORT: {
    abort();
    break;
  }
  case CRASH_UNCAUGHT_EXCEPTION: {
    ThrowException();
    break;
  }
#if XP_WIN && HAVE_64BIT_BUILD && !defined(__MINGW32__)
  case CRASH_X64CFI_UNKNOWN_OPCODE:
  case CRASH_X64CFI_PUSH_NONVOL:
  case CRASH_X64CFI_ALLOC_SMALL:
  case CRASH_X64CFI_ALLOC_LARGE:
  case CRASH_X64CFI_SAVE_NONVOL:
  case CRASH_X64CFI_SAVE_NONVOL_FAR:
  case CRASH_X64CFI_SAVE_XMM128:
  case CRASH_X64CFI_SAVE_XMM128_FAR:
  case CRASH_X64CFI_EPILOG: {
    auto m = GetWin64CFITestMap();
    if (m.find(how) == m.end()) {
      break;
    }
    auto pfnTest = m[how];
    auto pfnLauncher = m[CRASH_X64CFI_LAUNCHER];
    ReserveStack();
    pfnLauncher(0, pfnTest);
    break;
  }
#endif // XP_WIN && HAVE_64BIT_BUILD && !defined(__MINGW32__)
  default:
    break;
  }
}

char testData[32];

extern "C" NS_EXPORT
uint64_t SaveAppMemory()
{
  for (size_t i=0; i<sizeof(testData); i++)
    testData[i] = i;

  FILE *fp = fopen("crash-addr", "w");
  if (!fp)
    return 0;
  fprintf(fp, "%p\n", (void *)testData);
  fclose(fp);

  return (int64_t)testData;
}

#ifdef XP_WIN32
static LONG WINAPI HandleException(EXCEPTION_POINTERS* exinfo)
{
  TerminateProcess(GetCurrentProcess(), 0);
  return 0;
}

extern "C" NS_EXPORT
void TryOverrideExceptionHandler()
{
  SetUnhandledExceptionFilter(HandleException);
}
#endif

extern "C" NS_EXPORT uint32_t
GetWin64CFITestFnAddrOffset(int16_t fnid) {
#if XP_WIN && HAVE_64BIT_BUILD && !defined(__MINGW32__)
  // fnid uses the same constants as Crash().
  // Returns the RVA of the requested function.
  // Returns 0 on failure.
  auto m = GetWin64CFITestMap();
  if (m.find(fnid) == m.end()) {
    return 0;
  }
  uint64_t moduleBase = (uint64_t)GetModuleHandleW(L"testcrasher.dll");
  return ((uint64_t)m[fnid]) - moduleBase;
#else
  return 0;
#endif // XP_WIN && HAVE_64BIT_BUILD && !defined(__MINGW32__)
}
