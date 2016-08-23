/*
 * This is an adapted version of Android's original CursorLoader
 * without all the ContentProvider-specific bits.
 *
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.content.AsyncTaskLoader;

import org.mozilla.gecko.GeckoApplication;

/**
 * A copy of the framework's {@link android.content.CursorLoader} that
 * instead allows the caller to load the Cursor themselves via the abstract
 * {@link #loadCursor()} method, rather than calling out to a ContentProvider via
 * class methods.
 *
 * For new code, prefer {@link android.content.CursorLoader} (see @deprecated).
 *
 * This was originally created to re-use existing code which loaded Cursors manually.
 *
 * @deprecated since the framework provides an implementation, we'd like to eventually remove
 *             this class to reduce maintenance burden. Originally planned for bug 1239491, but
 *             it'd be more efficient to do this over time, rather than all at once.
 */
@Deprecated
public abstract class SimpleCursorLoader extends AsyncTaskLoader<Cursor> {
    final ForceLoadContentObserver mObserver;
    Cursor mCursor;

    public SimpleCursorLoader(Context context) {
        super(context);
        mObserver = new ForceLoadContentObserver();
    }

    /**
     * Loads the target cursor for this loader. This method is called
     * on a worker thread.
     */
    protected abstract Cursor loadCursor();

    /* Runs on a worker thread */
    @Override
    public Cursor loadInBackground() {
        Cursor cursor = loadCursor();

        if (cursor != null) {
            // Ensure the cursor window is filled
            cursor.getCount();
            cursor.registerContentObserver(mObserver);
        }

        return cursor;
    }

    /* Runs on the UI thread */
    @Override
    public void deliverResult(Cursor cursor) {
        if (isReset()) {
            // An async query came in while the loader is stopped
            if (cursor != null) {
                cursor.close();
            }

            return;
        }

        Cursor oldCursor = mCursor;
        mCursor = cursor;

        if (isStarted()) {
            super.deliverResult(cursor);
        }

        if (oldCursor != null && oldCursor != cursor && !oldCursor.isClosed()) {
            oldCursor.close();

            // Trying to read from the closed cursor will cause crashes, hence we should make
            // sure that no adapters/LoaderCallbacks are holding onto this cursor.
            GeckoApplication.getRefWatcher(getContext()).watch(oldCursor);
        }
    }

    /**
     * Starts an asynchronous load of the list data. When the result is ready the callbacks
     * will be called on the UI thread. If a previous load has been completed and is still valid
     * the result may be passed to the callbacks immediately.
     *
     * Must be called from the UI thread
     */
    @Override
    protected void onStartLoading() {
        if (mCursor != null) {
            deliverResult(mCursor);
        }

        if (takeContentChanged() || mCursor == null) {
            forceLoad();
        }
    }

    /**
     * Must be called from the UI thread
     */
    @Override
    protected void onStopLoading() {
        // Attempt to cancel the current load task if possible.
        cancelLoad();
    }

    @Override
    public void onCanceled(Cursor cursor) {
        if (cursor != null && !cursor.isClosed()) {
            cursor.close();
        }
    }

    @Override
    protected void onReset() {
        super.onReset();

        // Ensure the loader is stopped
        onStopLoading();

        if (mCursor != null && !mCursor.isClosed()) {
            mCursor.close();
        }

        mCursor = null;
    }
}