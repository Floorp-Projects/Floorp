/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browser.tests;

import org.mozilla.gecko.AppConstants;

import android.app.Activity;
import android.content.Context;
import android.test.ActivityInstrumentationTestCase2;

/**
 * BrowserTestCase provides helper methods for testing.
 */
public class BrowserTestCase extends ActivityInstrumentationTestCase2<Activity> {
    private static String LOG_TAG = "BrowserTestCase";

    private static final String LAUNCHER_ACTIVITY = AppConstants.ANDROID_PACKAGE_NAME + ".App";

    private final static Class<Activity> sLauncherActivityClass;

    static {
        try {
            sLauncherActivityClass = (Class<Activity>) Class.forName(LAUNCHER_ACTIVITY);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    public BrowserTestCase() {
        super(sLauncherActivityClass);
    }

    public Context getApplicationContext() {
        return this.getInstrumentation().getTargetContext().getApplicationContext();
    }
}
