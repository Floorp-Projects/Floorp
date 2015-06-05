/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.tests.browser.junit3.harness;

import android.os.Bundle;
import android.test.AndroidTestRunner;
import android.test.InstrumentationTestRunner;
import android.util.Log;

/**
 * A test runner that installs a special test listener.
 * <p>
 * In future, this listener will turn JUnit 3 test events into log messages in
 * the format that Mochitest parsers understand.
 */
public class BrowserInstrumentationTestRunner extends InstrumentationTestRunner {
    private static final String LOG_TAG = "BInstTestRunner";

    @Override
    public void onCreate(Bundle arguments) {
        Log.d(LOG_TAG, "onCreate");
        super.onCreate(arguments);
    }

    @Override
    protected AndroidTestRunner getAndroidTestRunner() {
        Log.d(LOG_TAG, "getAndroidTestRunner");
        AndroidTestRunner testRunner = super.getAndroidTestRunner();
        testRunner.addTestListener(new BrowserTestListener());
        return testRunner;
    }
}
