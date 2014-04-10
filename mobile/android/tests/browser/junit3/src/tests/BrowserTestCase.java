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
@SuppressWarnings("unchecked")
public class BrowserTestCase extends ActivityInstrumentationTestCase2<Activity> {
    @SuppressWarnings("unused")
    private static String LOG_TAG = "BrowserTestCase";

    /**
     * The Java Class instance that launches the browser.
     * <p>
     * This should always agree with {@link AppConstants#BROWSER_INTENT_CLASS_NAME}.
     */
    public static final Class<? extends Activity> BROWSER_INTENT_CLASS;

    // Use reflection here so we don't have to either (a) preprocess this
    // file, or (b) get access to Robocop's TestConstants class from these
    // instrumentation tests.
    static {
        Class<? extends Activity> cl;
        try {
            cl = (Class<? extends Activity>) Class.forName(AppConstants.BROWSER_INTENT_CLASS_NAME);
        } catch (ClassNotFoundException e) {
            // Oh well.
            cl = Activity.class;
        }
        BROWSER_INTENT_CLASS = cl;
    }

    public BrowserTestCase() {
        super((Class<Activity>) BROWSER_INTENT_CLASS);
    }

    public Context getApplicationContext() {
        return this.getInstrumentation().getTargetContext().getApplicationContext();
    }
}
