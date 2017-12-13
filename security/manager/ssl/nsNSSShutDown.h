/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSShutDown_h
#define nsNSSShutDown_h

#include "PLDHashTable.h"
#include "mozilla/Assertions.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "nscore.h"
#include "nspr.h"

class nsNSSShutDownObject;

// Singleton, owned by nsNSSShutDownList
class nsNSSActivityState
{
public:
  nsNSSActivityState();
  ~nsNSSActivityState();

  // Call enter/leave when PSM enters a scope during which
  // shutting down NSS is prohibited.
  void enter();
  void leave();
  // Wait for all activity to stop, and block any other thread on entering
  // relevant PSM code.
  PRStatus restrictActivityToCurrentThread();

  // Go back to normal state.
  void releaseCurrentThreadActivityRestriction();

private:
  // The lock protecting all our member variables.
  mozilla::Mutex mNSSActivityStateLock;

  // The activity variable, bound to our lock,
  // used either to signal the activity counter reaches zero,
  // or a thread restriction has been released.
  mozilla::CondVar mNSSActivityChanged;

  // The number of active scopes holding resources.
  int mNSSActivityCounter;

  // nullptr means "no restriction"
  // if not null, activity is only allowed on that thread
  PRThread* mNSSRestrictedThread;
};

// Helper class that automatically enters/leaves the global activity state
class nsNSSShutDownPreventionLock
{
public:
  nsNSSShutDownPreventionLock();
  ~nsNSSShutDownPreventionLock();
private:
  // Keeps track of whether or not we actually managed to enter the NSS activity
  // state. This is important because if we're attempting to shut down and we
  // can't enter the NSS activity state, we need to not attempt to leave it when
  // our destructor runs.
  bool mEnteredActivityState;
};

// Singleton, used by nsNSSComponent to track the list of PSM objects,
// which hold NSS resources and support the "early cleanup mechanism".
class nsNSSShutDownList
{
public:
  // track instances that support early cleanup
  static void remember(nsNSSShutDownObject *o);
  static void forget(nsNSSShutDownObject *o);

  // Release all tracked NSS resources and prevent nsNSSShutDownObjects from
  // using NSS functions.
  static nsresult evaporateAllNSSResourcesAndShutDown();

  // Signal entering/leaving a scope where shutting down NSS is prohibited.
  // enteredActivityState will be set to true if we actually managed to enter
  // the NSS activity state.
  static void enterActivityState(/*out*/ bool& enteredActivityState);
  static void leaveActivityState();

private:
  static bool construct(const mozilla::StaticMutexAutoLock& /*proofOfLock*/);

  nsNSSShutDownList();
  ~nsNSSShutDownList();

protected:
  PLDHashTable mObjects;
  nsNSSActivityState mActivityState;
};

/*
  A class deriving from nsNSSShutDownObject will have its instances
  automatically tracked in a list. However, it must follow some rules
  to assure correct behaviour.

  The tricky part is that it is not possible to call virtual
  functions from a destructor.

  The deriving class must override virtualDestroyNSSReference().
  Within this function, it should clean up all resources held to NSS.
  The function will be called by the global list, if it is time to
  shut down NSS before all references have been freed.

  The same code that goes into virtualDestroyNSSReference must
  also be called from the destructor of the deriving class,
  which is the standard cleanup (not called from the tracking list).

  Because of that duplication, it is suggested to implement a
  function destructorSafeDestroyNSSReference() in the deriving
  class, and make the implementation of virtualDestroyNSSReference()
  call destructorSafeDestroyNSSReference().

  The destructor of the derived class must prevent NSS shutdown on
  another thread by acquiring an nsNSSShutDownPreventionLock. It must
  then check to see if NSS has already been shut down by calling
  isAlreadyShutDown(). If NSS has not been shut down, the destructor
  must then call destructorSafeDestroyNSSReference() and then
  shutdown(ShutdownCalledFrom::Object). The second call will deregister with
  the tracking list, to ensure no additional attempt to free the resources
  will be made.

  ----------------------------------------------------------------------------
  IMPORTANT NOTE REGARDING CLASSES THAT IMPLEMENT nsNSSShutDownObject BUT DO
  NOT DIRECTLY HOLD NSS RESOURCES:
  ----------------------------------------------------------------------------
  Currently, classes that do not hold NSS resources but do call NSS functions
  inherit from nsNSSShutDownObject (and use the lock/isAlreadyShutDown
  mechanism) as a way of ensuring it is safe to call those functions. Because
  these classes do not hold any resources, however, it is tempting to skip the
  destructor component of this interface. This MUST NOT be done, because
  if an object of such a class is destructed before the nsNSSShutDownList
  processes all of its entries, this essentially causes a use-after-free when
  nsNSSShutDownList reaches the entry that has been destroyed. The safe way to
  do this is to implement the destructor as usual but omit the call to
  destructorSafeDestroyNSSReference() as it is unnecessary and probably isn't
  defined for that class.

  destructorSafeDestroyNSSReference() does not need to acquire an
  nsNSSShutDownPreventionLock or check isAlreadyShutDown() as long as it
  is only called by the destructor that has already acquired the lock and
  checked for shutdown or by the NSS shutdown code itself (which acquires
  the same lock and checks if objects it cleans up have already cleaned
  up themselves).

  destructorSafeDestroyNSSReference() MUST NOT cause any other
  nsNSSShutDownObject to be deconstructed. Doing so can cause
  unsupported concurrent operations on the hash table in the
  nsNSSShutDownList.

  class derivedClass : public nsISomeInterface,
                       public nsNSSShutDownObject
  {
    virtual void virtualDestroyNSSReference()
    {
      destructorSafeDestroyNSSReference();
    }

    void destructorSafeDestroyNSSReference()
    {
      // clean up all NSS resources here
    }

    virtual ~derivedClass()
    {
      nsNSSShutDownPreventionLock locker;
      if (isAlreadyShutDown()) {
        return;
      }
      destructorSafeDestroyNSSReference();
      shutdown(ShutdownCalledFrom::Object);
    }

    NS_IMETHODIMP doSomething()
    {
      if (isAlreadyShutDown())
        return NS_ERROR_NOT_AVAILABLE;

      // use the NSS resources and do something
    }
  };
*/

class nsNSSShutDownObject
{
public:
  enum class ShutdownCalledFrom {
    List,
    Object,
  };

  nsNSSShutDownObject()
  {
    mAlreadyShutDown = false;
    nsNSSShutDownList::remember(this);
  }

  virtual ~nsNSSShutDownObject()
  {
    // The derived class must call
    //   shutdown(ShutdownCalledFrom::Object);
    // in its destructor
  }

  void shutdown(ShutdownCalledFrom calledFrom)
  {
    if (!mAlreadyShutDown) {
      switch (calledFrom) {
        case ShutdownCalledFrom::Object:
          nsNSSShutDownList::forget(this);
          break;
        case ShutdownCalledFrom::List:
          virtualDestroyNSSReference();
          break;
        default:
          MOZ_CRASH("shutdown() called from an unknown source");
      }
      mAlreadyShutDown = true;
    }
  }

  bool isAlreadyShutDown() const;

protected:
  virtual void virtualDestroyNSSReference() = 0;
private:
  volatile bool mAlreadyShutDown;
};

#endif // nsNSSShutDown_h
