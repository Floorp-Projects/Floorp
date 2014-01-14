/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _INC_NSSShutDown_H
#define _INC_NSSShutDown_H

#include "nscore.h"
#include "nspr.h"
#include "pldhash.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

class nsNSSShutDownObject;
class nsOnPK11LogoutCancelObject;

// Singleton, owner by nsNSSShutDownList
class nsNSSActivityState
{
public:
  nsNSSActivityState();
  ~nsNSSActivityState();

  // Call enter/leave when PSM enters a scope during which
  // shutting down NSS is prohibited.
  void enter();
  void leave();
  
  // Call enter/leave when PSM is about to show a UI
  // while still holding resources.
  void enterBlockingUIState();
  void leaveBlockingUIState();
  
  // Is the activity aware of any blocking PSM UI currently shown?
  bool isBlockingUIActive();

  // Is it forbidden to bring up an UI while holding resources?
  bool isUIForbidden();
  
  // Check whether setting the current thread restriction is possible.
  // If it is possible, and the "do_it_for_real" flag is used,
  // the state tracking will have ensured that we will stay in this state.
  // As of writing, this includes forbidding PSM UI.
  enum RealOrTesting {test_only, do_it_for_real};
  bool ifPossibleDisallowUI(RealOrTesting rot);

  // Notify the state tracking that going to the restricted state is
  // no longer planned.
  // As of writing, this includes clearing the "PSM UI forbidden" flag.
  void allowUI();

  // If currently no UI is shown, wait for all activity to stop,
  // and block any other thread on entering relevant PSM code.
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

  // The number of scopes holding resources while blocked
  // showing an UI.
  int mBlockingUICounter;

  // Whether bringing up UI is currently forbidden
  bool mIsUIForbidden;

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
};

// Helper class that automatically enters/leaves the global UI tracking
class nsPSMUITracker
{
public:
  nsPSMUITracker();
  ~nsPSMUITracker();
  
  bool isUIForbidden();
};

// Singleton, used by nsNSSComponent to track the list of PSM objects,
// which hold NSS resources and support the "early cleanup mechanism".
class nsNSSShutDownList
{
public:
  ~nsNSSShutDownList();

  static nsNSSShutDownList *construct();
  
  // track instances that support early cleanup
  static void remember(nsNSSShutDownObject *o);
  static void forget(nsNSSShutDownObject *o);

  // track instances that would like notification when
  // a PK11 logout operation is performed.
  static void remember(nsOnPK11LogoutCancelObject *o);
  static void forget(nsOnPK11LogoutCancelObject *o);

  // track the creation and destruction of SSL sockets
  // performed by clients using PSM services
  static void trackSSLSocketCreate();
  static void trackSSLSocketClose();
  static bool areSSLSocketsActive();
  
  // Are we able to do the early cleanup?
  // Returns failure if at the current time "early cleanup" is not possible.
  bool isUIActive();

  // If possible to do "early cleanup" at the current time, remember that we want to
  // do it, and disallow actions that would change the possibility.
  bool ifPossibleDisallowUI();

  // Notify that it is no longer planned to do the "early cleanup".
  void allowUI();
  
  // Do the "early cleanup", if possible.
  nsresult evaporateAllNSSResources();

  // PSM has been asked to log out of a token.
  // Notify all registered instances that want to react to that event.
  nsresult doPK11Logout();
  
  static nsNSSActivityState *getActivityState()
  {
    return singleton ? &singleton->mActivityState : nullptr;
  }
  
private:
  nsNSSShutDownList();
  static PLDHashOperator
  evaporateAllNSSResourcesHelper(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                                        uint32_t number, void *arg);

  static PLDHashOperator
  doPK11LogoutHelper(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                                    uint32_t number, void *arg);
protected:
  mozilla::Mutex mListLock;
  static nsNSSShutDownList *singleton;
  PLDHashTable mObjects;
  uint32_t mActiveSSLSockets;
  PLDHashTable mPK11LogoutCancelObjects;
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
  shutdown(calledFromObject). The second call will deregister with
  the tracking list, to ensure no additional attempt to free the resources
  will be made.

  destructorSafeDestroyNSSReference() does not need to acquire an
  nsNSSShutDownPreventionLock or check isAlreadyShutDown() as long as it
  is only called by the destructor that has already acquired the lock and
  checked for shutdown or by the NSS shutdown code itself (which acquires
  the same lock and checks if objects it cleans up have already cleaned
  up themselves).

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
      shutdown(calledFromObject);
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

  enum CalledFromType {calledFromList, calledFromObject};

  nsNSSShutDownObject()
  {
    mAlreadyShutDown = false;
    nsNSSShutDownList::remember(this);
  }
  
  virtual ~nsNSSShutDownObject()
  {
    // the derived class must call 
    //   shutdown(calledFromObject);
    // in its destructor
  }
  
  void shutdown(CalledFromType calledFrom)
  {
    if (!mAlreadyShutDown) {
      if (calledFromObject == calledFrom) {
        nsNSSShutDownList::forget(this);
      }
      if (calledFromList == calledFrom) {
        virtualDestroyNSSReference();
      }
      mAlreadyShutDown = true;
    }
  }
  
  bool isAlreadyShutDown() { return mAlreadyShutDown; }

protected:
  virtual void virtualDestroyNSSReference() = 0;
private:
  volatile bool mAlreadyShutDown;
};

class nsOnPK11LogoutCancelObject
{
public:
  nsOnPK11LogoutCancelObject()
  :mIsLoggedOut(false)
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

#endif
