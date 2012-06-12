/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

import android.content.Context;

/**
 * I perform a sync.
 *
 * Initialize me by calling `load` with a SynchronizerConfiguration.
 *
 * Start synchronizing by calling `synchronize` with a SynchronizerDelegate. I
 * provide coarse-grained feedback by calling my delegate's callback methods.
 *
 * I always call exactly one of my delegate's `onSynchronized` or
 * `onSynchronizeFailed` callback methods. In addition, I call
 * `onSynchronizeAborted` before `onSynchronizeFailed` when I encounter a fetch,
 * store, or session error while synchronizing.
 *
 * After synchronizing, call `save` to get back a SynchronizerConfiguration with
 * updated bundle information.
 */
public class Synchronizer implements SynchronizerSessionDelegate {
  public static final String LOG_TAG = "SyncDelSDelegate";

  protected String configSyncID; // Used to pass syncID from load() back into save().

  protected SynchronizerDelegate synchronizerDelegate;

  @Override
  public void onInitialized(SynchronizerSession session) {
    session.synchronize();
  }

  @Override
  public void onSynchronized(SynchronizerSession synchronizerSession) {
    Logger.debug(LOG_TAG, "Got onSynchronized.");
    Logger.debug(LOG_TAG, "Notifying SynchronizerDelegate.");
    this.synchronizerDelegate.onSynchronized(synchronizerSession.getSynchronizer());
  }

  @Override
  public void onSynchronizeSkipped(SynchronizerSession synchronizerSession) {
    Logger.debug(LOG_TAG, "Got onSynchronizeSkipped.");
    Logger.debug(LOG_TAG, "Notifying SynchronizerDelegate as if on success.");
    this.synchronizerDelegate.onSynchronized(synchronizerSession.getSynchronizer());
  }

  @Override
  public void onSynchronizeFailed(SynchronizerSession session,
      Exception lastException, String reason) {
    this.synchronizerDelegate.onSynchronizeFailed(session.getSynchronizer(), lastException, reason);
  }

  public Repository repositoryA;
  public Repository repositoryB;
  public RepositorySessionBundle bundleA;
  public RepositorySessionBundle bundleB;

  /**
   * Fetch a synchronizer session appropriate for this <code>Synchronizer</code>
   */
  public SynchronizerSession getSynchronizerSession() {
    return new SynchronizerSession(this, this);
  }

  /**
   * Start synchronizing, calling delegate's callback methods.
   */
  public void synchronize(Context context, SynchronizerDelegate delegate) {
    this.synchronizerDelegate = delegate;
    SynchronizerSession session = getSynchronizerSession();
    session.init(context, bundleA, bundleB);
  }

  public SynchronizerConfiguration save() {
    return new SynchronizerConfiguration(configSyncID, bundleA, bundleB);
  }

  /**
   * Set my repository session bundles from a SynchronizerConfiguration.
   *
   * This method is not thread-safe.
   *
   * @param config
   */
  public void load(SynchronizerConfiguration config) {
    bundleA = config.remoteBundle;
    bundleB = config.localBundle;
    configSyncID  = config.syncID;
  }
}
