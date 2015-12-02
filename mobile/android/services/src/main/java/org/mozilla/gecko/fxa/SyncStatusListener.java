/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import android.accounts.Account;
import android.content.Context;
import android.support.annotation.UiThread;

/**
 * Interface definition for a callback to be invoked when an sync status change.
 */
public interface SyncStatusListener {
    public Context getContext();
    public Account getAccount();

    /**
     * Called when sync has started.
     * This is always called in UiThread.
     */
    @UiThread
    public void onSyncStarted();

    /**
     * Called when sync has finished.
     * This is always called in UiThread.
     */
    @UiThread
    public void onSyncFinished();
}