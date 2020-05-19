package org.mozilla.focus.screenshots;

import android.app.Instrumentation;
import android.content.Context;
import androidx.annotation.StringRes;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.uiautomator.UiDevice;
import android.text.format.DateUtils;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.rules.TestRule;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule;
import org.mozilla.focus.helpers.SessionLoadedIdlingResource;
import org.mozilla.focus.utils.AppConstants;

import tools.fastlane.screengrab.Screengrab;
import tools.fastlane.screengrab.UiAutomatorScreenshotStrategy;

/**
 * Base class for tests that take screenshots.
 */
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
abstract class ScreenshotTest {
    final long waitingTime = DateUtils.SECOND_IN_MILLIS * 10;

    private Context targetContext;
    private SessionLoadedIdlingResource loadingIdlingResource;

    UiDevice device;

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule = new MainActivityFirstrunTestRule(false) {
        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();
        }
    };

    @Rule
    public TestRule screenshotOnFailureRule = new TestWatcher() {
        @Override
        protected void failed(Throwable e, Description description) {
            // On error take a screenshot so that we can debug it easily
            Screengrab.screenshot("FAILURE-" + getScreenshotName(description));
        }

        private String getScreenshotName(Description description) {
            return description.getClassName().replace(".", "-")
                    + "_"
                    + description.getMethodName().replace(".", "-");
        }
    };

    @Before
    public void setUpScreenshots() {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        targetContext = instrumentation.getTargetContext();
        device = UiDevice.getInstance(instrumentation);

        // Use this to switch between default strategy and HostScreencap strategy
        Screengrab.setDefaultScreenshotStrategy(new UiAutomatorScreenshotStrategy());
        //Screengrab.setDefaultScreenshotStrategy(new HostScreencapScreenshotStrategy(device));

        device.waitForIdle();
    }

    /* Disable idlingResources.  This causes error when accessing Settings Dialog */
    /*
    @Before
    public void setUpIdlingResources() {
        loadingIdlingResource = new SessionLoadedIdlingResource();
        IdlingRegistry.getInstance().register(loadingIdlingResource);
    }

    @After
    public void tearDownIdlingResources() {
        device.waitForIdle();
        IdlingRegistry.getInstance().unregister(loadingIdlingResource);
    }
    */
    String getString(@StringRes int resourceId) {
        return targetContext.getString(resourceId).trim();
    }

    String getString(@StringRes int resourceId, Object... formatArgs) {
        return targetContext.getString(resourceId, formatArgs).trim();
    }
}
