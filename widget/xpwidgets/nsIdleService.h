/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Gijs Kruitbosch <gijskruitbosch@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Gijs Kruitbosch <gijskruitbosch@gmail.com>
 *  Mike Kristoffersen <mozstuff@mikek.dk>
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

#ifndef nsIdleService_h__
#define nsIdleService_h__

#include "nsIIdleService.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsIIdleService.h"
#include "nsCategoryCache.h"
#include "nsWeakReference.h"

/**
 * Class we can use to store an observer with its associated idle time
 * requirement and whether or not the observer thinks it's "idle".
 */
class IdleListener {
public:
  nsCOMPtr<nsIObserver> observer;
  PRUint32 reqIdleTime;
  bool isIdle;

  IdleListener(nsIObserver* obs, PRUint32 reqIT, bool aIsIdle = false) :
    observer(obs), reqIdleTime(reqIT), isIdle(aIsIdle) {}
  ~IdleListener() {}
};

// This one will be declared later.
class nsIdleService;

/**
 * Class to handle the daily idle timer.
 */
class nsIdleServiceDaily : public nsIObserver,
                           public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsIdleServiceDaily(nsIIdleService* aIdleService);

  /**
   * Initializes the daily idle observer.
   * Keep this separated from the constructor, since it could cause pointer
   * corruption due to AddRef/Release of "this".
   */
  void Init();

  virtual ~nsIdleServiceDaily();

private:
  /**
   * @note This is a normal pointer, part to avoid creating a cycle with the
   * idle service, part to avoid potential pointer corruption due to this class
   * being instantiated in the constructor of the service itself.
   */
  nsIIdleService* mIdleService;

  /**
   * Set to true when the instantiated object has a idle observer.
   */
  bool mObservesIdle;

  /**
   * Place to hold the timer used by this class to determine when a day has
   * passed, after that it will wait for idle time to be detected.
   */
  nsCOMPtr<nsITimer> mTimer;

  /**
   * Function that is called back once a day.
   */
  static void DailyCallback(nsITimer* aTimer, void* aClosure);

  /**
   * Cache of observers for the "idle-daily" category.
   */
  nsCategoryCache<nsIObserver> mCategoryObservers;

  /**
   * Boolean set to true when daily idle notifications should be disabled.
   */
  bool mShutdownInProgress;
};

class nsIdleService : public nsIIdleService
{
public:
  nsIdleService();

  // Implement nsIIdleService methods.
  NS_IMETHOD AddIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime);
  NS_IMETHOD RemoveIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime);
  NS_IMETHOD GetIdleTime(PRUint32* idleTime);

  /**
   * Function that resets the idle time in the service, in other words it
   * sets the time for the last user interaction to now, or now-idleDelta
   *
   * @param idleDelta the time (in milliseconds) since the last user inter
   *                  action
   **/
  void ResetIdleTimeOut(PRUint32 idleDeltaInMS = 0);

protected:
  virtual ~nsIdleService();

  /**
   * If there is a platform specific function to poll the system idel time
   * then that must be returned in this function, and the function MUST return
   * true, otherwise then the function should be left unimplemented or made
   * to return false (this can also be used for systems where it depends on
   * the configuration of the system if the idle time can be determined)
   *
   * @param aIdleTime
   *        The idle time in ms.
   *
   * @return true if the idle time could be polled, false otherwise.
   *
   * @note The time returned by this function can be different than the one
   *       returned by GetIdleTime, as that is corrected by any calls to
   *       ResetIdleTimeOut(), unless you overwrite that function too...
   */
  virtual bool PollIdleTime(PRUint32* aIdleTime);

  /**
   * Function that determines if we are in poll mode or not.
   *
   * @return true if polling is supported, false otherwise.
   */
  virtual bool UsePollMode();

private:
  /**
   * Ensure that the timer is expiring at least at the given time
   *
   * The function might not restart the timer if there is one running currently
   *
   * @param aNextTimeoutInPR
   *        The last absolute time the timer should expire
   */
  void SetTimerExpiryIfBefore(PRTime aNextTimeoutInPR);

  /**
   * Stores the next timeout time, 0 means timer not running
   */
  PRTime mCurrentlySetToTimeoutAtInPR;

  /**
   * mTimer holds the internal timer used by this class to detect when to poll
   * for idle time, when to check if idle timers should expire etc.
   */
  nsCOMPtr<nsITimer> mTimer;

  /**
   * Array of listeners that wants to be notified about idle time.
   */
  nsTArray<IdleListener> mArrayListeners;

  /**
   * Object keeping track of the daily idle thingy.
   */
  nsRefPtr<nsIdleServiceDaily> mDailyIdle;

  /**
   * Boolean indicating if any observers are in idle mode
   */
  bool mAnyObserverIdle;

  /**
   * Delta time from last non idle time to when the next observer should switch
   * to idle mode
   *
   * Time in seconds
   *
   * If this value is 0 it means there are no active observers
   */
  PRUint32 mDeltaToNextIdleSwitchInS;

  /**
   * Absolute value for when the last user interaction took place.
   */
  PRTime mLastUserInteractionInPR;


  /**
   * Function that ensures the timer is running with at least the minimum time
   * needed.  It will kill the timer if there are no active observers.
   */
  void ReconfigureTimer(void);

  /**
   * Callback function that is called when the internal timer expires.
   *
   * This in turn calls the IdleTimerCallback that does the real processing
   */
  static void StaticIdleTimerCallback(nsITimer* aTimer, void* aClosure);

  /**
   * Function that handles when a timer has expired
   */
  void IdleTimerCallback(void);
};

#endif // nsIdleService_h__
