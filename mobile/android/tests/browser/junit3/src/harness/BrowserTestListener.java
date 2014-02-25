/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.browser.harness;

import junit.framework.AssertionFailedError;
import junit.framework.Test;
import junit.framework.TestListener;
import android.util.Log;

/**
 * BrowserTestListener turns JUnit 3 test events into log messages in the format
 * that Mochitest parsers understand.
 * <p>
 * The idea is that, on infrastructure, we'll be able to use the same test
 * parsing code for Browser JUnit 3 tests as we do for Robocop tests.
 * <p>
 * In future, that is!
 */
public class BrowserTestListener implements TestListener {
    public static final String LOG_TAG = "BTestListener";

    @Override
    public void startTest(Test test) {
        Log.d(LOG_TAG, "startTest: " + test);
    }

    @Override
    public void endTest(Test test) {
        Log.d(LOG_TAG, "endTest: " + test);
    }

    @Override
    public void addFailure(Test test, AssertionFailedError t) {
        Log.d(LOG_TAG, "addFailure: " + test);
    }

    @Override
    public void addError(Test test, Throwable t) {
        Log.d(LOG_TAG, "addError: " + test);
    }
}
