#include "nsServiceManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsITestCrasher.h"
#include "nsXULAppAPI.h"
#include "mozilla/ModuleUtils.h"

class nsTestCrasher : public nsITestCrasher
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITESTCRASHER

  nsTestCrasher() {}

private:
  ~nsTestCrasher() {};
};


NS_IMPL_ISUPPORTS1(nsTestCrasher, nsITestCrasher)

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

/* void crash (); */
NS_IMETHODIMP nsTestCrasher::Crash(PRInt16 how)
{
  switch (how) {
  case nsITestCrasher::CRASH_INVALID_POINTER_DEREF: {
    volatile int* foo = (int*)0x42;
    *foo = 0;
    // not reached
    break;
  }
  case nsITestCrasher::CRASH_PURE_VIRTUAL_CALL: {
    PureVirtualCall();
    // not reached
    break;
  }
  case nsITestCrasher::CRASH_RUNTIMEABORT: {
    NS_RUNTIMEABORT("Intentional crash");
    break;
  }
  default:
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

/* nsISupports LockDir (in nsILocalFile directory); */
NS_IMETHODIMP nsTestCrasher::LockDir(nsILocalFile *directory,
                                     nsISupports **_retval NS_OUTPARAM)
{
  return XRE_LockProfileDirectory(directory, _retval);
}

// 54afce51-38d7-4df0-9750-2f90f9ffbca2
#define NS_TESTCRASHER_CID \
{ 0x54afce51, 0x38d7, 0x4df0, {0x97, 0x50, 0x2f, 0x90, 0xf9, 0xff, 0xbc, 0xa2} }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTestCrasher)
NS_DEFINE_NAMED_CID(NS_TESTCRASHER_CID);

static const mozilla::Module::CIDEntry kTestCrasherCIDs[] = {
  { &kNS_TESTCRASHER_CID, false, NULL, nsTestCrasherConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kTestCrasherContracts[] = {
  { "@mozilla.org/testcrasher;1", &kNS_TESTCRASHER_CID },
  { NULL }
};

static const mozilla::Module kTestCrasherModule = {
  mozilla::Module::kVersion,
  kTestCrasherCIDs,
  kTestCrasherContracts
};

NSMODULE_DEFN(nsTestCrasherModule) = &kTestCrasherModule;
