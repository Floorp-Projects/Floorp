/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.PowerManager;
import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;
import android.util.Log;

import com.robotium.solo.Solo;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.Driver;
import org.mozilla.gecko.FennecInstrumentationTestRunner;
import org.mozilla.gecko.FennecMochitestAssert;
import org.mozilla.gecko.FennecNativeActions;
import org.mozilla.gecko.FennecNativeDriver;
import org.mozilla.gecko.FennecTalosAssert;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.updater.UpdateServiceHelper;

import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Map;

@SuppressWarnings("unchecked")
public abstract class BaseRobocopTest extends ActivityInstrumentationTestCase2<Activity> {
    public static final String LOGTAG = "BaseTest";

    public enum Type {
        MOCHITEST,
        TALOS
    }

    public static final String DEFAULT_ROOT_PATH = "/mnt/sdcard/tests";

    // How long to wait for a Robocop:Quit message to actually kill Fennec.
    private static final int ROBOCOP_QUIT_WAIT_MS = 180000;

    /**
     * The Java Class instance that launches the browser.
     * <p>
     * This should always agree with {@link AppConstants#MOZ_ANDROID_BROWSER_INTENT_CLASS}.
     */
    public static final Class<? extends Activity> BROWSER_INTENT_CLASS;

    // Use reflection here so we don't have to preprocess this file.
    static {
        Class<? extends Activity> cl;
        try {
            cl = (Class<? extends Activity>) Class.forName(AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
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

    // Member function to allow specialization.
    protected Intent createActivityIntent() {
        return BaseRobocopTest.createActivityIntent(mConfig);
    }

    // Static function to allow re-use.
    public static Intent createActivityIntent(Map<String, String> config) {
        final Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.putExtra("args", "-no-remote -profile " + config.get("profile"));
        // Don't show the first run experience.
        intent.putExtra(BrowserApp.EXTRA_SKIP_STARTPANE, true);

        final String envString = config.get("envvars");
        if (!TextUtils.isEmpty(envString)) {
            final String[] envStrings = envString.split(",");

            for (int iter = 0; iter < envStrings.length; iter++) {
                intent.putExtra("env" + iter, envStrings[iter]);
            }
        }

        return intent;
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

    @Override
    protected void runTest() throws Throwable {
        try {
            super.runTest();
        } catch (Throwable t) {
            // save screenshot -- written to /mnt/sdcard/Robotium-Screenshots
            // as <filename>.jpg
            mSolo.takeScreenshot("robocop-screenshot-"+getClass().getName());
            if (mAsserter != null) {
                mAsserter.dumpLog("Exception caught during test!", t);
                mAsserter.ok(false, "Exception caught", t.toString());
            }
            // re-throw to continue bail-out
            throw t;
        }
    }

    @Override
    public void tearDown() throws Exception {
        try {
            mAsserter.endTest();

            // By default, we don't quit Fennec on finish, and we don't finish
            // all opened activities. Not quiting Fennec entirely is intended to
            // make life better for local testers, who might want to alter a
            // test that is under development rather than Fennec itself. Not
            // finishing activities is intended to allow local testers to
            // manually inspect an activity's state after a test
            // run. runtestsremote.py sets this to "1".  Testers running via an
            // IDE will not have this set at all.
            final String quitAndFinish = FennecInstrumentationTestRunner.getFennecArguments()
                    .getString("quit_and_finish"); // null means not specified.
            if ("1".equals(quitAndFinish)) {
                // Request the browser force quit and wait for it to take effect.
                Log.i(LOGTAG, "Requesting force quit.");
                mActions.sendGlobalEvent("Robocop:Quit", null);
                mSolo.sleep(ROBOCOP_QUIT_WAIT_MS);

                // If still running, finish activities as recommended by Robotium.
                Log.i(LOGTAG, "Finishing all opened activities.");
                mSolo.finishOpenedActivities();
            } else {
                // This has the effect of keeping the activity-under-test
                // around; if we don't set it to null, it is killed, either by
                // finishOpenedActivities above or super.tearDown below.
                Log.i(LOGTAG, "Not requesting force quit and trying to keep started activity alive.");
                setActivity(null);
            }
        } catch (Throwable e) {
            e.printStackTrace();
        }
        super.tearDown();
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
        final String rawUrl = mConfig.get("rawhost").replaceAll("(/$)", "");

        HttpURLConnection urlConnection = null;

        try {
            urlConnection = (HttpURLConnection) new URL(rawUrl).openConnection();

            final int statusCode = urlConnection.getResponseCode();
            if (200 != statusCode) {
                throw new IllegalStateException("Status code: " + statusCode);
            }
        } catch (Exception e) {
            mAsserter.ok(false, "Robocop tests on your device need network/wifi access to reach: [" + rawUrl + "].", e.toString());
        } finally {
            if (urlConnection != null) {
                urlConnection.disconnect();
            }
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

    protected GeckoProfile getTestProfile() {
        if (mProfile.startsWith("/")) {
            return GeckoProfile.get(getActivity(), /* profileName */ null, mProfile);
        }

        return GeckoProfile.get(getActivity(), mProfile);
    }
}
