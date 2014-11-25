/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.database.Cursor;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;

import org.mozilla.gecko.animation.TransitionsTracker;

/**
 * A {@link LoaderCallbacks} implementation that avoids running its
 * {@link #onLoadFinished(Loader, Cursor)} method during animations as it's
 * likely to trigger a layout traversal as a result of a cursor swap in the
 * target adapter.
 */
public abstract class TransitionAwareCursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
    private OnLoadFinishedRunnable onLoadFinishedRunnable;

    @Override
    public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
        if (onLoadFinishedRunnable != null) {
            TransitionsTracker.cancelPendingAction(onLoadFinishedRunnable);
        }

        onLoadFinishedRunnable = new OnLoadFinishedRunnable(loader, c);
        TransitionsTracker.runAfterTransitions(onLoadFinishedRunnable);
    }

    protected abstract void onLoadFinishedAfterTransitions(Loader<Cursor> loade, Cursor c);

    @Override
    public void onLoaderReset(Loader<Cursor> loader) {
        if (onLoadFinishedRunnable != null) {
            TransitionsTracker.cancelPendingAction(onLoadFinishedRunnable);
            onLoadFinishedRunnable = null;
        }
    }

    private class OnLoadFinishedRunnable implements Runnable {
        private final Loader<Cursor> loader;
        private final Cursor cursor;

        public OnLoadFinishedRunnable(Loader<Cursor> loader, Cursor cursor) {
            this.loader = loader;
            this.cursor = cursor;
        }

        @Override
        public void run() {
            onLoadFinishedAfterTransitions(loader, cursor);
            onLoadFinishedRunnable = null;
        }
    }
}