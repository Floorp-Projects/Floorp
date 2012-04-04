/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;

import android.app.Activity;
import android.app.Application;

public class GeckoApplication extends Application {

    private ArrayList<ApplicationLifecycleCallbacks> mListeners;

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
        if (!activity.isApplicationInBackground())
            return;

        if (mListeners == null)
            return;

        for (ApplicationLifecycleCallbacks listener: mListeners)
            listener.onApplicationPause();
    }

    public void onActivityResume(GeckoActivity activity) {
        // This is a misnomer. Should have been "wasApplicationInBackground".
        if (!activity.isApplicationInBackground())
            return;

        if (mListeners == null)
            return;

        for (ApplicationLifecycleCallbacks listener: mListeners)
            listener.onApplicationResume();
    }
}
