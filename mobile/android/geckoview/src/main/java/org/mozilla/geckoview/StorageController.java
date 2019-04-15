/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * Manage and control runtime storage data.
 *
 * Retrieve an instance via {@link GeckoRuntime#getStorageController}.
 */
public final class StorageController {
    /**
     * Clear all browser storage data like cookies and localStorage for the
     * given context.
     *
     * Note: Any open session may re-accumulate previously cleared data. To
     * ensure that no persistent data is left behind, you need to close all
     * sessions for the given context prior to clearing data.
     *
     * @param contextId The context ID for the storage data to be deleted.
     *                  For null, all storage data will be cleared.
     */
    @AnyThread
    public void clearSessionContextData(final @NonNull String contextId) {
        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("contextId", contextId);

        EventDispatcher.getInstance().dispatch(
            "GeckoView:ClearSessionContextData", bundle);
    }

    /**
     * Clear all browser storage data like cookies and localStorage.
     *
     * Note: Any open session may re-accumulate previously cleared data. To
     * ensure that no persistent data is left behind, you need to close all
     * sessions prior to clearing data.
     */
    @AnyThread
    public void clearAllSessionContextData() {
        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("contextId", null);

        EventDispatcher.getInstance().dispatch(
            "GeckoView:ClearSessionContextData", bundle);
    }
}
