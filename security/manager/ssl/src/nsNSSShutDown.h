/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kaie@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  // nsnull means "no restriction"
  // if != nsnull, activity is only allowed on that thread
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
    return singleton ? &singleton->mActivityState : nsnull;
  }
  
private:
  nsNSSShutDownList();
  static PLDHashOperator PR_CALLBACK
  evaporateAllNSSResourcesHelper(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                                        PRUint32 number, void *arg);

  static PLDHashOperator PR_CALLBACK
  doPK11LogoutHelper(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                                    PRUint32 number, void *arg);
protected:
  mozilla::Mutex mListLock;
  static nsNSSShutDownList *singleton;
  PLDHashTable mObjects;
  PRUint32 mActiveSSLSockets;
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

  The destructor of the derived class should call 
  destructorSafeDestroyNSSReference() and afterwards call
  shutdown(calledFromObject), in order to deregister with the
  tracking list, to ensure no additional attempt to free the resources
  will be made.
  
  Function destructorSafeDestroyNSSReference() must
  also ensure, that NSS resources have not been freed already.
  To achieve this, the deriving class should call 
  isAlreadyShutDown() to check.
  
  It is important that you make your implementation
  failsafe, and check whether the resources have already been freed,
  in each function that requires the resources.
  
  class derivedClass : public nsISomeInterface,
                       public nsNSSShutDownObject
  {
    virtual void virtualDestroyNSSReference()
    {
      destructorSafeDestroyNSSReference();
    }
    
    void destructorSafeDestroyNSSReference()
    {
      if (isAlreadyShutDown())
        return;
      
      // clean up all NSS resources here
    }

    virtual ~derivedClass()
    {
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
    mAlreadyShutDown = PR_FALSE;
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
      mAlreadyShutDown = PR_TRUE;
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
  :mIsLoggedOut(PR_FALSE)
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
    
    mIsLoggedOut = PR_TRUE;
  }
  
  bool isPK11LoggedOut()
  {
    return mIsLoggedOut;
  }

private:
  volatile bool mIsLoggedOut;
};

#endif
