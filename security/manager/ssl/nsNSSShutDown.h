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
class nsOnPK11LogoutCancelObject;

// Yes, this races. We don't care because we're only temporarily using this to
// silence compiler warnings. Without it every instance of a
// nsNSSShutDownPreventionLock would be unused, causing the compiler to
// complain. This will be removed when we've demonstrated that not shutting down
// NSS is feasible (and beneficial) and we can remove the machinery entirely in
// bug 1421084.
static int sSilenceCompilerWarnings;

class nsNSSShutDownPreventionLock
{
public:
  nsNSSShutDownPreventionLock()
  {
    sSilenceCompilerWarnings++;
  }

  ~nsNSSShutDownPreventionLock()
  {
    sSilenceCompilerWarnings--;
  }
};

// Singleton, used by nsNSSComponent to track the list of PSM objects,
// which hold NSS resources and support the "early cleanup mechanism".
class nsNSSShutDownList
{
public:
  // track instances that would like notification when
  // a PK11 logout operation is performed.
  static void remember(nsOnPK11LogoutCancelObject *o);
  static void forget(nsOnPK11LogoutCancelObject *o);

  // Release all tracked NSS resources and prevent nsNSSShutDownObjects from
  // using NSS functions.
  static nsresult evaporateAllNSSResourcesAndShutDown();

  // PSM has been asked to log out of a token.
  // Notify all registered instances that want to react to that event.
  static nsresult doPK11Logout();

private:
  static bool construct(const mozilla::StaticMutexAutoLock& /*proofOfLock*/);

  nsNSSShutDownList();
  ~nsNSSShutDownList();

protected:
  PLDHashTable mPK11LogoutCancelObjects;
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
  }

  virtual ~nsNSSShutDownObject()
  {
    // The derived class must call
    //   shutdown(ShutdownCalledFrom::Object);
    // in its destructor
  }

  void shutdown(ShutdownCalledFrom)
  {
  }

  bool isAlreadyShutDown() const;

protected:
  virtual void virtualDestroyNSSReference() = 0;
};

class nsOnPK11LogoutCancelObject
{
public:
  nsOnPK11LogoutCancelObject()
    : mIsLoggedOut(false)
  {
    nsNSSShutDownList::remember(this);
  }

  virtual ~nsOnPK11LogoutCancelObject()
  {
    nsNSSShutDownList::forget(this);
  }

  void logout()
  {
    // We do not care for a race condition.
    // Once the bool arrived at false,
    // later calls to isPK11LoggedOut() will see it.
    // This is a one-time change from 0 to 1.
    mIsLoggedOut = true;
  }

  bool isPK11LoggedOut()
  {
    return mIsLoggedOut;
  }

private:
  volatile bool mIsLoggedOut;
};

#endif // nsNSSShutDown_h
