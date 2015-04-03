/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.Map;

import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;
import org.mozilla.gecko.Actions;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.Driver;
import org.mozilla.gecko.FennecInstrumentationTestRunner;
import org.mozilla.gecko.FennecMochitestAssert;
import org.mozilla.gecko.FennecNativeActions;
import org.mozilla.gecko.FennecNativeDriver;
import org.mozilla.gecko.FennecTalosAssert;
import org.mozilla.gecko.updater.UpdateServiceHelper;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.PowerManager;
import android.test.ActivityInstrumentationTestCase2;
import android.util.Log;

import com.jayway.android.robotium.solo.Solo;

@SuppressWarnings("unchecked")
public abstract class BaseRobocopTest extends ActivityInstrumentationTestCase2<Activity> {
    public enum Type {
        MOCHITEST,
        TALOS
    }

    private static final String DEFAULT_ROOT_PATH = "/mnt/sdcard/tests";

    /**
     * The Java Class instance that launches the browser.
     * <p>
     * This should always agree with {@link AppConstants#BROWSER_INTENT_CLASS_NAME}.
     */
    public static final Class<? extends Activity> BROWSER_INTENT_CLASS;

    // Use reflection here so we don't have to preprocess this file.
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

    protected Assert mAsserter;
    protected String mLogFile;

    protected String mBaseHostnameUrl;
    protected String mBaseIpUrl;

    protected Map<String, String> mConfig;
    protected String mRootPath;

    protected Solo mSolo;
    protected Driver mDriver;
    protected Actions mActions;

    protected String mProfile;

    protected StringHelper mStringHelper;

    protected abstract Intent createActivityIntent();

    /**
     * The browser is started at the beginning of this test. A single test is a
     * class inheriting from <code>BaseRobocopTest</code> that contains test
     * methods.
     * <p>
     * If a test should not start the browser at the beginning of a test,
     * specify a different activity class to the one-argument constructor. To do
     * as little as possible, specify <code>Activity.class</code>.
     */
    public BaseRobocopTest() {
        this((Class<Activity>) BROWSER_INTENT_CLASS);
    }

    /**
     * Start the given activity class at the beginning of this test.
     * <p>
     * <b>You should use the no-argument constructor in almost all cases.</b>
     *
     * @param activityClass to start before this test.
     */
    protected BaseRobocopTest(Class<Activity> activityClass) {
        super(activityClass);
    }

    /**
     * Returns the test type: mochitest or talos.
     * <p>
     * By default tests are mochitests, but a test can override this method in
     * order to change its type. Most Robocop tests are mochitests.
     */
    protected Type getTestType() {
        return Type.MOCHITEST;
    }

    @Override
    protected void setUp() throws Exception {
        // Disable the updater.
        UpdateServiceHelper.setEnabled(false);

        // Load config file from root path (set up by Python script).
        mRootPath = FennecInstrumentationTestRunner.getFennecArguments().getString("deviceroot");
        if (mRootPath == null) {
            Log.w("Robocop", "Did not find deviceroot in arguments; falling back to: " + DEFAULT_ROOT_PATH);
            mRootPath = DEFAULT_ROOT_PATH;
        }
        String configFile = FennecNativeDriver.getFile(mRootPath + "/robotium.config");
        mConfig = FennecNativeDriver.convertTextToTable(configFile);
        mLogFile = mConfig.get("logfile");
        mProfile = mConfig.get("profile");
        mBaseHostnameUrl = mConfig.get("host").replaceAll("(/$)", "");
        mBaseIpUrl = mConfig.get("rawhost").replaceAll("(/$)", "");

        // Initialize the asserter.
        if (getTestType() == Type.TALOS) {
            mAsserter = new FennecTalosAssert();
        } else {
            mAsserter = new FennecMochitestAssert();
        }
        mAsserter.setLogFile(mLogFile);
        mAsserter.setTestName(getClass().getName());

        // Start the activity.
        final Intent intent = createActivityIntent();
        setActivityIntent(intent);

        // Set up Robotium.solo and Driver objects
        Activity tempActivity = getActivity();

        StringHelper.initialize(tempActivity.getResources());
        mStringHelper = StringHelper.get();

        mSolo = new Solo(getInstrumentation(), tempActivity);
        mDriver = new FennecNativeDriver(tempActivity, mSolo, mRootPath);
        mActions = new FennecNativeActions(tempActivity, mSolo, getInstrumentation(), mAsserter);

    }

    /**
     * Function to early abort if we can't reach the given HTTP server. Provides local testers
     * with diagnostic information. Not currently available for TALOS tests, which are rarely run
     * locally in any case.
     */
    public void throwIfHttpGetFails() {
        if (getTestType() == Type.TALOS) {
            return;
        }

        // rawURL to test fetching from. This should be a raw (IP) URL, not an alias
        // (like mochi.test). We can't (easily) test fetching from the aliases, since
        // those are managed by Fennec's proxy settings.
        final String rawUrl = ((String) mConfig.get("rawhost")).replaceAll("(/$)", "");

        try {
            final HttpClient httpclient = new DefaultHttpClient();
            final HttpResponse response = httpclient.execute(new HttpGet(rawUrl));
            final int statusCode = response.getStatusLine().getStatusCode();
            if (200 != statusCode) {
                throw new IllegalStateException("Status code: " + statusCode);
            }
        } catch (Exception e) {
            mAsserter.ok(false, "Robocop tests on your device need network/wifi access to reach: [" + rawUrl + "].", e.toString());
        }
    }

    /**
     * Ensure that the screen on the test device is powered on during tests.
     */
    public void throwIfScreenNotOn() {
        final PowerManager pm = (PowerManager) getActivity().getSystemService(Context.POWER_SERVICE);
        mAsserter.ok(pm.isScreenOn(),
            "Robocop tests need the test device screen to be powered on.", "");
    }
}
