#include "mozilla/Assertions.h"

#include <stdio.h>

#include "nscore.h"
#include "nsXULAppAPI.h"
#include "nsExceptionHandler.h"
#include "mozilla/unused.h"

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
  void f() { }
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

// Keep these in sync with CrashTestUtils.jsm!
const int16_t CRASH_INVALID_POINTER_DEREF = 0;
const int16_t CRASH_PURE_VIRTUAL_CALL     = 1;
const int16_t CRASH_RUNTIMEABORT          = 2;
const int16_t CRASH_OOM                   = 3;
const int16_t CRASH_MOZ_CRASH             = 4;
const int16_t CRASH_ABORT                 = 5;

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
  case CRASH_RUNTIMEABORT: {
    NS_RUNTIMEABORT("Intentional crash");
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
  default:
    break;
  }
}

extern "C" NS_EXPORT
nsISupports* LockDir(nsIFile *directory)
{
  nsISupports* lockfile = nullptr;
  XRE_LockProfileDirectory(directory, &lockfile);
  return lockfile;
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
