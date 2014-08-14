/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import java.util.Map;
import java.util.Map.Entry;
import java.util.WeakHashMap;

import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;

import android.content.ContentResolver;
import android.content.SyncStatusObserver;

/**
 * Abstract away some details of Android's SyncStatusObserver.
 * <p>
 * Provides a simplified sync started/sync finished delegate.
 */
public class FxAccountSyncStatusHelper implements SyncStatusObserver {
  @SuppressWarnings("unused")
  private static final String LOG_TAG = FxAccountSyncStatusHelper.class.getSimpleName();

  protected static FxAccountSyncStatusHelper sInstance = null;

  public synchronized static FxAccountSyncStatusHelper getInstance() {
    if (sInstance == null) {
      sInstance = new FxAccountSyncStatusHelper();
    }
    return sInstance;
  }

  // Used to unregister this as a listener.
  protected Object handle = null;

  // Maps delegates to whether their underlying Android account was syncing the
  // last time we observed a status change.
  protected Map<FirefoxAccounts.SyncStatusListener, Boolean> delegates = new WeakHashMap<FirefoxAccounts.SyncStatusListener, Boolean>();

  @Override
  public synchronized void onStatusChanged(int which) {
    for (Entry<FirefoxAccounts.SyncStatusListener, Boolean> entry : delegates.entrySet()) {
      final FirefoxAccounts.SyncStatusListener delegate = entry.getKey();
      final AndroidFxAccount fxAccount = new AndroidFxAccount(delegate.getContext(), delegate.getAccount());
      final boolean active = fxAccount.isCurrentlySyncing();
      // Remember for later.
      boolean wasActiveLastTime = entry.getValue();
      // It's okay to update the value of an entry while iterating the entrySet.
      entry.setValue(active);

      if (active && !wasActiveLastTime) {
        // We've started a sync.
        delegate.onSyncStarted();
      }
      if (!active && wasActiveLastTime) {
        // We've finished a sync.
        delegate.onSyncFinished();
      }
    }
  }

  protected void addListener() {
    final int mask = ContentResolver.SYNC_OBSERVER_TYPE_ACTIVE;
    if (this.handle != null) {
      throw new IllegalStateException("Already registered this as an observer?");
    }
    this.handle = ContentResolver.addStatusChangeListener(mask, this);
  }

  protected void removeListener() {
    Object handle = this.handle;
    this.handle = null;
    if (handle != null) {
      ContentResolver.removeStatusChangeListener(handle);
    }
  }

  public synchronized void startObserving(FirefoxAccounts.SyncStatusListener delegate) {
    if (delegate == null) {
      throw new IllegalArgumentException("delegate must not be null");
    }
    if (delegates.containsKey(delegate)) {
      return;
    }
    // If we are the first delegate to the party, start listening.
    if (delegates.isEmpty()) {
      addListener();
    }
    delegates.put(delegate, Boolean.FALSE);
  }

  public synchronized void stopObserving(FirefoxAccounts.SyncStatusListener delegate) {
    delegates.remove(delegate);
    // If we are the last delegate leaving the party, stop listening.
    if (delegates.isEmpty()) {
      removeListener();
    }
  }
}
