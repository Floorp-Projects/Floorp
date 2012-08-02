/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Application;

import java.util.ArrayList;

public class GeckoApplication extends Application {

    private boolean mInBackground = false;
    private ArrayList<ApplicationLifecycleCallbacks> mListeners;

    @Override
    public void onCreate() {
        // workaround for http://code.google.com/p/android/issues/detail?id=20915
        try {
            Class.forName("android.os.AsyncTask");
        } catch (ClassNotFoundException e) {}

        super.onCreate();
    }

    public interface ApplicationLifecycleCallbacks {
        public void onApplicationPause();
        public void onApplicationResume();
    }

    public void addApplicationLifecycleCallbacks(ApplicationLifecycleCallbacks callback) {
        if (mListeners == null)
            mListeners = new ArrayList<ApplicationLifecycleCallbacks>();

        mListeners.add(callback);
    }

    public void removeApplicationLifecycleCallbacks(ApplicationLifecycleCallbacks callback) {
        if (mListeners == null)
            return;

        mListeners.remove(callback);
    }

    public void onActivityPause(GeckoActivity activity) {
        if (activity.isGeckoActivityOpened())
            return;

        if (mListeners == null)
            return;

        mInBackground = true;

        for (ApplicationLifecycleCallbacks listener: mListeners)
            listener.onApplicationPause();
    }

    public void onActivityResume(GeckoActivity activity) {
        // This is a misnomer. Should have been "wasGeckoActivityOpened".
        if (activity.isGeckoActivityOpened())
            return;

        if (mListeners == null)
            return;

        for (ApplicationLifecycleCallbacks listener: mListeners)
            listener.onApplicationResume();

        mInBackground = false;
    }

    public boolean isApplicationInBackground() {
        return mInBackground;
    }
}
