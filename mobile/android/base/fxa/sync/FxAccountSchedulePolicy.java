/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State.Action;
import org.mozilla.gecko.sync.BackoffHandler;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;

public class FxAccountSchedulePolicy implements SchedulePolicy {
  private static final String LOG_TAG = "FxAccountSchedulePolicy";

  // Our poll intervals are used to trigger automatic background syncs
  // in the absence of user activity.

  // If we're waiting for the user to click on a verification link, we
  // sync very often in order to detect a change in state.
  //
  // In the case of unverified -> unverified (no transition), this should be
  // very close to a single HTTP request (with the SyncAdapter overhead, of
  // course, but that's not wildly different from alarm manager overhead).
  //
  // The /account/status endpoint is HAWK authed by sessionToken, so we still
  // have to do some crypto no matter what.

  // TODO: only do this for a while...
  public static final long POLL_INTERVAL_PENDING_VERIFICATION = 60;         // 1 minute.

  // If we're in some kind of error state, there's no point trying often.
  // This is not the same as a server-imposed backoff, which will be
  // reflected dynamically.
  public static final long POLL_INTERVAL_ERROR_STATE = 24 * 60 * 60;        // 24 hours.

  // If we're the only device, just sync a few times a day in case that
  // changes.
  public static final long POLL_INTERVAL_SINGLE_DEVICE_SEC = 8 * 60 * 60;   // 8 hours.

  // And if we know there are other devices, let's sync often enough that
  // we'll likely be caught up (even if not completely) by the time you
  // next use this device.
  public static final long POLL_INTERVAL_MULTI_DEVICE_SEC = 30 * 60;        // 30 minutes.

  // Never sync more frequently than this, unless forced.
  public static final long POLL_INTERVAL_MINIMUM_SEC = 45;                  // 45 seconds.

  // This is used solely as an optimization for backoff handling, so it's not
  // persisted.
  private static volatile long POLL_INTERVAL_CURRENT_SEC = POLL_INTERVAL_SINGLE_DEVICE_SEC;

  private final AndroidFxAccount account;
  private final Context context;

  public FxAccountSchedulePolicy(Context context, AndroidFxAccount account) {
    this.account = account;
    this.context = context;
  }

  /**
   * Return a millisecond timestamp in the future, offset from the current
   * time by the provided amount.
   * @param millis the duration by which to delay
   * @return a timestamp.
   */
  private static long delay(long millis) {
    return System.currentTimeMillis() + millis;
  }

  /**
   * Updates the existing system periodic sync interval to the specified duration.
   *
   * @param intervalSeconds the requested period, which Android will vary by up to 4%.
   */
  protected void requestPeriodicSync(final long intervalSeconds) {
    final String authority = BrowserContract.AUTHORITY;
    final Account account = this.account.getAndroidAccount();
    this.context.getContentResolver();
    Logger.info(LOG_TAG, "Scheduling periodic sync for " + intervalSeconds + ".");
    ContentResolver.addPeriodicSync(account, authority, Bundle.EMPTY, intervalSeconds);
    POLL_INTERVAL_CURRENT_SEC = intervalSeconds;
  }

  @Override
  public void onSuccessfulSync(int otherClientsCount) {
    // This undoes the change made in observeBackoffMillis -- once we hit backoff we'll
    // periodically sync at the backoff duration, but as soon as we succeed we'll switch
    // into the client-count-dependent interval.
    long interval = (otherClientsCount > 0) ? POLL_INTERVAL_MULTI_DEVICE_SEC : POLL_INTERVAL_SINGLE_DEVICE_SEC;
    requestPeriodicSync(interval);
  }

  @Override
  public void onHandleFinal(Action needed) {
    switch (needed) {
    case NeedsPassword:
    case NeedsUpgrade:
      requestPeriodicSync(POLL_INTERVAL_ERROR_STATE);
      break;
    case NeedsVerification:
      requestPeriodicSync(POLL_INTERVAL_PENDING_VERIFICATION);
      break;
    case None:
      // No action needed: we'll set the periodic sync interval
      // when the sync finishes, via the SessionCallback.
      break;
    }
  }

  @Override
  public void onUpgradeRequired() {
    // TODO: this shouldn't occur in FxA, but when we upgrade we
    // need to reduce the interval again.
    requestPeriodicSync(POLL_INTERVAL_ERROR_STATE);
  }

  @Override
  public void onUnauthorized() {
    // TODO: this shouldn't occur in FxA, but when we fix our credentials
    // we need to reduce the interval again.
    requestPeriodicSync(POLL_INTERVAL_ERROR_STATE);
  }

  @Override
  public void configureBackoffMillisOnBackoff(BackoffHandler backoffHandler, long backoffMillis, boolean onlyExtend) {
    if (onlyExtend) {
      backoffHandler.extendEarliestNextRequest(delay(backoffMillis));
    } else {
      backoffHandler.setEarliestNextRequest(delay(backoffMillis));
    }

    // Yes, we might be part-way through the interval, in which case the backoff
    // code will do its job. But we certainly don't want to reduce the interval
    // if we're given a small backoff instruction.
    // We'll reset the poll interval next time we sync without a backoff instruction.
    if (backoffMillis > (POLL_INTERVAL_CURRENT_SEC * 1000)) {
      // Slightly inflate the backoff duration to ensure that a fuzzed
      // periodic sync doesn't occur before our backoff has passed. Android
      // 19+ default to a 4% fuzz factor.
      requestPeriodicSync((long) Math.ceil((1.05 * backoffMillis) / 1000));
    }
  }

  @Override
  public void configureBackoffMillisBeforeSyncing(BackoffHandler backoffHandler) {
    backoffHandler.setEarliestNextRequest(delay(POLL_INTERVAL_MINIMUM_SEC * 1000));
  }
}