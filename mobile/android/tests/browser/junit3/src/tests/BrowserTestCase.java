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
    @SuppressWarnings("unused")
    private static String LOG_TAG = "BrowserTestCase";

    @SuppressWarnings("unchecked")
    public BrowserTestCase() {
        super((Class<Activity>) AppConstants.BROWSER_INTENT_CLASS);
    }

    public Context getApplicationContext() {
        return this.getInstrumentation().getTargetContext().getApplicationContext();
    }
}
