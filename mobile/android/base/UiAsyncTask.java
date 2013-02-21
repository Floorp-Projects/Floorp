/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Handler;

// AsyncTask runs onPostExecute on the thread it is constructed on
// We construct these off of the main thread, and we want that to run
// on the main UI thread, so this is a convenience class to do that
public abstract class UiAsyncTask<Params, Progress, Result> {
    private volatile boolean mCancelled = false;
    private final Handler mBackgroundThreadHandler;
    private final Handler mUiHandler;

    public UiAsyncTask(Handler uiHandler, Handler backgroundThreadHandler) {
        mUiHandler = uiHandler;
        mBackgroundThreadHandler = backgroundThreadHandler;
    }

    private final class BackgroundTaskRunnable implements Runnable {
        private Params[] mParams;

        public BackgroundTaskRunnable(Params... params) {
            mParams = params;
        }

        public void run() {
            final Result result = doInBackground(mParams);

            mUiHandler.post(new Runnable() {
                public void run() {
                    if (mCancelled)
                        onCancelled();
                    else
                        onPostExecute(result);
                }
            });
        }
    }

    public final void execute(final Params... params) {
        mUiHandler.post(new Runnable() {
            public void run() {
                onPreExecute();
                mBackgroundThreadHandler.post(new BackgroundTaskRunnable(params));
            }
        });
    }

    @SuppressWarnings({"UnusedParameters"})
    public final boolean cancel(boolean mayInterruptIfRunning) {
        mCancelled = true;
        return mCancelled;
    }

    public final boolean isCancelled() {
        return mCancelled;
    }

    protected void onPreExecute() { }
    protected void onPostExecute(Result result) { }
    protected void onCancelled() { }
    protected abstract Result doInBackground(Params... params);
}
