/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import org.mozilla.gecko.fxa.login.State.Action;
import org.mozilla.gecko.sync.BackoffHandler;

public interface SchedulePolicy {
  /**
   * Call this with the number of other clients syncing to the account.
   */
  public abstract void onSuccessfulSync(int otherClientsCount);
  public abstract void onHandleFinal(Action needed);
  public abstract void onUpgradeRequired();
  public abstract void onUnauthorized();

  /**
   * Before a sync we typically wish to adjust our backoff policy. This cleans
   * the slate prior to encountering a new backoff, and also functions as a rate
   * limiter.
   *
   * The {@link SchedulePolicy} acts as a controller for the {@link BackoffHandler}.
   * As a result of calling these two methods, the {@link BackoffHandler} will be
   * mutated, and additional side-effects (such as scheduling periodic syncs) can
   * occur.
   *
   * @param rateHandler the backoff handler to configure for basic rate limiting.
   * @param backgroundHandler the backoff handler to configure for background operations.
   */
  public abstract void configureBackoffMillisBeforeSyncing(BackoffHandler rateHandler, BackoffHandler backgroundHandler);

  /**
   * We received an explicit backoff instruction, typically from a server.
   *
   * @param onlyExtend
   *          if <code>true</code>, the backoff handler will be asked to update
   *          its backoff only if the provided value is greater than the current
   *          backoff.
   */
  public abstract void configureBackoffMillisOnBackoff(BackoffHandler backoffHandler, long backoffMillis, boolean onlyExtend);
}