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
 *
 * We use generics differently to Android's AsyncTask.
 * Android uses a "Params" type parameter to represent the type of all the parameters to this task.
 * It then uses arguments of type Params... to permit arbitrarily-many of these to be passed
 * fluently.
 *
 * Unfortunately, since Java does not support generic array types (and since varargs desugars to a
 * single array parameter) that behaviour exposes a hole in the type system. See:
 * http://docs.oracle.com/javase/tutorial/java/generics/nonReifiableVarargsType.html#vulnerabilities
 *
 * Instead, we equivalently have a single type parameter "Param". A UiAsyncTask may take exactly one
 * parameter of type Param. Since Param can be an array type, this no more restrictive than the
 * other approach, it just provides additional type safety.
 */
public abstract class UIAsyncTask<Param, Result> {
    /**
     * Provide a convenient API for parameter-free UiAsyncTasks by wrapping parameter-taking methods
     * from UiAsyncTask in parameterless equivalents.
     */
    public static abstract class WithoutParams<InnerResult> extends UIAsyncTask<Void, InnerResult> {
        public WithoutParams(Handler backgroundThreadHandler) {
            super(backgroundThreadHandler);
        }

        public void execute() {
            execute(null);
        }

        @Override
        protected InnerResult doInBackground(Void unused) {
            return doInBackground();
        }

        protected abstract InnerResult doInBackground();
    }

    final Handler mBackgroundThreadHandler;
    private volatile boolean mCancelled;
    private static Handler sHandler;

    /**
     * Creates a new asynchronous task.
     *
     * @param backgroundThreadHandler the handler to execute the background task on
     */
    public UIAsyncTask(Handler backgroundThreadHandler) {
        mBackgroundThreadHandler = backgroundThreadHandler;
    }

    private static synchronized Handler getUiHandler() {
        if (sHandler == null) {
            sHandler = new Handler(Looper.getMainLooper());
        }

        return sHandler;
    }

    private final class BackgroundTaskRunnable implements Runnable {
        private final Param mParam;

        public BackgroundTaskRunnable(Param param) {
            mParam = param;
        }

        @Override
        public void run() {
            final Result result = doInBackground(mParam);

            getUiHandler().post(new Runnable() {
                @Override
                public void run() {
                    if (mCancelled) {
                        onCancelled();
                    } else {
                        onPostExecute(result);
                    }
                }
            });
        }
    }

    protected void execute(final Param param) {
        getUiHandler().post(new Runnable() {
            @Override
            public void run() {
                onPreExecute();
                mBackgroundThreadHandler.post(new BackgroundTaskRunnable(param));
            }
        });
    }

    public final boolean cancel() {
        mCancelled = true;
        return mCancelled;
    }

    public final boolean isCancelled() {
        return mCancelled;
    }

    protected void onPreExecute() { }
    protected void onPostExecute(Result result) { }
    protected void onCancelled() { }
    protected abstract Result doInBackground(Param param);
}
