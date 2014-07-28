/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.os.Handler;
import android.os.Looper;

/**
 * Executes a background task and publishes the result on the UI thread.
 *
 * The standard {@link android.os.AsyncTask} only runs onPostExecute on the
 * thread it is constructed on, so this is a convenience class for creating
 * tasks off the UI thread.
 */
public abstract class UiAsyncTask<Params, Progress, Result> {
    private volatile boolean mCancelled;
    private final Handler mBackgroundThreadHandler;
    private static Handler sHandler;

    /**
     * Creates a new asynchronous task.
     *
     * @param backgroundThreadHandler the handler to execute the background task on
     */
    public UiAsyncTask(Handler backgroundThreadHandler) {
        mBackgroundThreadHandler = backgroundThreadHandler;
    }

    private static synchronized Handler getUiHandler() {
        if (sHandler == null) {
            sHandler = new Handler(Looper.getMainLooper());
        }
        return sHandler;
    }

    private final class BackgroundTaskRunnable implements Runnable {
        private Params[] mParams;

        public BackgroundTaskRunnable(Params... params) {
            mParams = params;
        }

        @Override
        public void run() {
            final Result result = doInBackground(mParams);

            getUiHandler().post(new Runnable() {
                @Override
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
        getUiHandler().post(new Runnable() {
            @Override
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
