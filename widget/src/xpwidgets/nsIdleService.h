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
 *  Mike Kristoffersen <moz@mikek.dk>
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
class nsIdleServiceDaily : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsIdleServiceDaily(nsIdleService* aIdleService);

  /**
   * This function will make this class release its allocated resources (its
   * idle timer and/or its normal timer).
   */
  void Shutdown();

private:
  /**
   * @note This is a normal pointer, or the idle service could keep it self
   * alive.
   */
  nsIdleService* mIdleService;

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
};

class nsIdleService : public nsIIdleService
{
public:
  nsIdleService();

  // Implement nsIIdleService methods.
  NS_IMETHOD AddIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime);
  NS_IMETHOD RemoveIdleObserver(nsIObserver* aObserver, PRUint32 aIdleTime);
  NS_IMETHOD GetIdleTime(PRUint32* idleTime);

  void ResetIdleTimeOut();

protected:
  ~nsIdleService();

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

  /**
   * Send expired events and start timers.
   *
   * @param aNoTimeReset
   *        If true new times will not be calculated.
   */
  void CheckAwayState(bool aNoTimeReset);

private:
  /**
   * Start the internal timer, restart it if it is allready running.
   *
   * @param aDelay
   *        The time in seconds that should pass before the next timeout.
   */

  void StartTimer(PRUint32 aDelay);

  /**
   * Stop the internal timer, it is safe to call this function, even when
   * there are no timers running.
   */
  void StopTimer();

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
  nsCOMPtr<nsIdleServiceDaily> mDailyIdle;

  /**
   * Contains the time of the last idle reset or 0 if there haven't been a
   * reset.
   * <p>
   * Time is kept in seconds since the epoch at midnight, January 1, 1970 UTC.
   */
  PRUint32 mLastIdleReset;

  /**
   * The time since the last handled activity (which might be different than
   * mLastIdleReset, since the activity that reset the idle timer could just
   * have happend, and not handled yet).
   * <p>
   * Time is kept in seconds since the epoch at midnight, January 1, 1970 UTC.
   */
  PRUint32 mLastHandledActivity;

  /**
   * Callback function that is called when the internal timer expires.
   */
  static void IdleTimerCallback(nsITimer* aTimer, void* aClosure);

  /**
   * Whether the idle time calculated in the last call to GetIdleTime is
   * actually valid (see nsIdleService.idl - we return 0 when it isn't).
   */
  bool mPolledIdleTimeIsValid;
};

#endif // nsIdleService_h__
