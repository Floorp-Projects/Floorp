#include "nscore.h"
#include "nsXULAppAPI.h"

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
};

void fcn( A* p )
{
  p->f();
}

void PureVirtualCall()
{
  // generates a pure virtual function call
  B b;
}

// Keep these in sync with CrashTestUtils.jsm!
const PRInt16 CRASH_INVALID_POINTER_DEREF = 0;
const PRInt16 CRASH_PURE_VIRTUAL_CALL     = 1;
const PRInt16 CRASH_RUNTIMEABORT          = 2;

extern "C" NS_EXPORT
void Crash(PRInt16 how)
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
  default:
    break;
  }
}

extern "C" NS_EXPORT
nsISupports* LockDir(nsILocalFile *directory)
{
  nsISupports* lockfile = nsnull;
  XRE_LockProfileDirectory(directory, &lockfile);
  return lockfile;
}
