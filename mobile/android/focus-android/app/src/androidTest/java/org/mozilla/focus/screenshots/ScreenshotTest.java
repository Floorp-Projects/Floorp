package org.mozilla.focus.screenshots;

import android.app.Instrumentation;
import android.content.Context;
import android.support.annotation.StringRes;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.IdlingRegistry;
import android.support.test.uiautomator.UiDevice;
import android.text.format.DateUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.rules.TestRule;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.mozilla.focus.helpers.HostScreencapScreenshotStrategy;
import org.mozilla.focus.helpers.SessionLoadedIdlingResource;

import tools.fastlane.screengrab.Screengrab;

/**
 * Base class for tests that take screenshots.
 */
abstract class ScreenshotTest {
    final long waitingTime = DateUtils.SECOND_IN_MILLIS * 10;

    private Context targetContext;
    private SessionLoadedIdlingResource loadingIdlingResource;

    UiDevice device;

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
        //Screengrab.setDefaultScreenshotStrategy(new UiAutomatorScreenshotStrategy());
        Screengrab.setDefaultScreenshotStrategy(new HostScreencapScreenshotStrategy(device));

        device.waitForIdle();
    }

    @Before
    public void setUpIdlingResources() {
        loadingIdlingResource = new SessionLoadedIdlingResource();
        IdlingRegistry.getInstance().register(loadingIdlingResource);
    }

    @After
    public void tearDownIdlingResources() {
        IdlingRegistry.getInstance().unregister(loadingIdlingResource);
    }

    String getString(@StringRes int resourceId) {
        return targetContext.getString(resourceId).trim();
    }

    String getString(@StringRes int resourceId, Object... formatArgs) {
        return targetContext.getString(resourceId, formatArgs).trim();
    }
}
