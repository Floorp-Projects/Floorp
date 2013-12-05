/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Assert;
import org.mozilla.gecko.Driver;
import org.mozilla.gecko.FennecInstrumentationTestRunner;
import org.mozilla.gecko.FennecMochitestAssert;
import org.mozilla.gecko.FennecNativeActions;
import org.mozilla.gecko.FennecNativeDriver;
import org.mozilla.gecko.FennecTalosAssert;
import org.mozilla.gecko.TestConstants;
import org.mozilla.gecko.tests.components.*;
import org.mozilla.gecko.tests.helpers.*;

import com.jayway.android.robotium.solo.Solo;

import android.app.Activity;
import android.content.Intent;
import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;

import java.util.HashMap;

/**
 * A base test class for Robocop (UI-centric) tests. This and the related classes attempt to
 * provide a framework to improve upon the issues discovered with the previous BaseTest
 * implementation by providing simple test authorship and framework extension, consistency,
 * and reliability.
 */
abstract class UITest extends ActivityInstrumentationTestCase2<Activity>
                      implements UITestContext {

    protected enum Type {
        MOCHITEST,
        TALOS
    }

    private static final String LAUNCHER_ACTIVITY = TestConstants.ANDROID_PACKAGE_NAME + ".App";
    private static final String TARGET_PACKAGE_ID = "org.mozilla.gecko";

    private final static Class<Activity> sLauncherActivityClass;

    private Activity mActivity;
    private Solo mSolo;
    private Driver mDriver;
    private Actions mActions;
    private Assert mAsserter;

    // Base to build hostname URLs
    private String mBaseHostnameUrl;
    // Base to build IP URLs
    private String mBaseIpUrl;

    protected AboutHomeComponent mAboutHome;
    protected ToolbarComponent mToolbar;

    static {
        try {
            sLauncherActivityClass = (Class<Activity>) Class.forName(LAUNCHER_ACTIVITY);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    public UITest() {
        super(sLauncherActivityClass);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        final String rootPath = FennecInstrumentationTestRunner.getFennecArguments().getString("deviceroot");
        final HashMap config = loadConfigTable(rootPath);
        final Intent intent = createActivityIntent(config);
        setActivityIntent(intent);

        // Start the activity.
        mActivity = getActivity();

        if (getTestType() == Type.TALOS) {
            mAsserter = new FennecTalosAssert();
        } else {
            mAsserter = new FennecMochitestAssert();
        }

        final String logFile = (String) config.get("logfile");
        mAsserter.setLogFile(logFile);
        mAsserter.setTestName(this.getClass().getName());

        mSolo = new Solo(getInstrumentation(), mActivity);
        mDriver = new FennecNativeDriver(mActivity, mSolo, rootPath);
        mActions = new FennecNativeActions(mActivity, mSolo, getInstrumentation(), mAsserter);

        mBaseHostnameUrl = ((String) config.get("host")).replaceAll("(/$)", "");
        mBaseIpUrl = ((String) config.get("rawhost")).replaceAll("(/$)", "");

        // Helpers depend on components so initialize them first.
        initComponents();
        initHelpers();
    }

    @Override
    public void tearDown() throws Exception {
        try {
            mAsserter.endTest();
            mSolo.finishOpenedActivities();
        } catch (Throwable e) {
            e.printStackTrace();
        }

        super.tearDown();
    }

    private void initComponents() {
        mAboutHome = new AboutHomeComponent(this);
        mToolbar = new ToolbarComponent(this);
    }

    private void initHelpers() {
        // Other helpers make assertions so init AssertionHelper first.
        AssertionHelper.init(this);

        DeviceHelper.init(this);
        GeckoHelper.init(this);
        GestureHelper.init(this);
        NavigationHelper.init(this);
        WaitHelper.init(this);
    }

    @Override
    public Solo getSolo() {
        return mSolo;
    }

    @Override
    public Assert getAsserter() {
        return mAsserter;
    }

    @Override
    public Driver getDriver() {
        return mDriver;
    }

    @Override
    public Actions getActions() {
        return mActions;
    }

    @Override
    public void dumpLog(final String message) {
        mAsserter.dumpLog(message);
    }

    @Override
    public void dumpLog(final String message, final Throwable t) {
        mAsserter.dumpLog(message, t);
    }

    @Override
    public BaseComponent getComponent(final ComponentType type) {
        switch (type) {
            case ABOUTHOME:
                return mAboutHome;

            case TOOLBAR:
                return mToolbar;

            default:
                fail("Unknown component type, " + type + ".");
                return null; // Should not reach this statement but required by javac.
        }
    }

    /**
     * Returns the test type. By default this returns MOCHITEST, but tests can override this
     * method in order to change the type of the test.
     */
    protected Type getTestType() {
        return Type.MOCHITEST;
    }

    @Override
    public String getAbsoluteHostnameUrl(final String url) {
        return getAbsoluteUrl(mBaseHostnameUrl, url);
    }

    @Override
    public String getAbsoluteIpUrl(final String url) {
        return getAbsoluteUrl(mBaseIpUrl, url);
    }

    private String getAbsoluteUrl(final String baseUrl, final String url) {
        return baseUrl + "/" + url.replaceAll("(^/)", "");
    }

    private static HashMap loadConfigTable(final String rootPath) {
        final String configFile = FennecNativeDriver.getFile(rootPath + "/robotium.config");
        return FennecNativeDriver.convertTextToTable(configFile);
    }

    private static Intent createActivityIntent(final HashMap config) {
        final Intent intent = new Intent(Intent.ACTION_MAIN);

        final String profile = (String) config.get("profile");
        intent.putExtra("args", "-no-remote -profile " + profile);

        final String envString = (String) config.get("envvars");
        if (!TextUtils.isEmpty(envString)) {
            final String[] envStrings = envString.split(",");

            for (int iter = 0; iter < envStrings.length; iter++) {
                intent.putExtra("env" + iter, envStrings[iter]);
            }
        }

        return intent;
    }
}
