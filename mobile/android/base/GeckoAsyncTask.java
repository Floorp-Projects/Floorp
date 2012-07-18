/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

// AsyncTask runs onPostExecute on the thread it is constructed on
// We construct these off of the main thread, and we want that to run
// on the main UI thread, so this is a convenience class to do that
public abstract class GeckoAsyncTask<Params, Progress, Result> {
    public static final int PRIORITY_NORMAL = 0;
    public static final int PRIORITY_HIGH = 1;

    private int mPriority;

    public GeckoAsyncTask() {
        mPriority = PRIORITY_NORMAL;
    }

    private class BackgroundTaskRunnable implements Runnable {
        private Params[] mParams;

        public BackgroundTaskRunnable(Params... params) {
            mParams = params;
        }

        public void run() {
            final Result result = doInBackground(mParams);
            GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                public void run() {
                    onPostExecute(result);
                }
            });
        }
    }

    public void execute(final Params... params) {
        if (mPriority == PRIORITY_HIGH)
            GeckoAppShell.getHandler().postAtFrontOfQueue(new BackgroundTaskRunnable(params));
        else
            GeckoAppShell.getHandler().post(new BackgroundTaskRunnable(params));
    }

    public GeckoAsyncTask<Params, Progress, Result> setPriority(int priority) {
        mPriority = priority;
        return this;
    }

    protected abstract Result doInBackground(Params... params);
    protected abstract void  onPostExecute(Result result);
}
