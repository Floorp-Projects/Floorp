// /* This Source Code Form is subject to the terms of the Mozilla Public
//  * License, v. 2.0. If a copy of the MPL was not distributed with this
//  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//
// package org.mozilla.fenix.ui.screenshots;
//
// import android.Manifest;
// import android.app.Instrumentation;
// import android.content.Context;
// import androidx.annotation.StringRes;
// import androidx.test.platform.app.InstrumentationRegistry;
// import androidx.test.rule.GrantPermissionRule;
// import androidx.test.uiautomator.UiDevice;
// import android.text.format.DateUtils;
//
// import org.junit.Before;
// import org.junit.Rule;
// import org.junit.rules.TestRule;
// import org.junit.rules.TestWatcher;
// import org.junit.runner.Description;
//
// import tools.fastlane.screengrab.Screengrab;
// import tools.fastlane.screengrab.UiAutomatorScreenshotStrategy;
//
// /**
//  * Base class for tests that take screenshots.
//  */
//  public abstract class ScreenshotTest {
//     final long waitingTime = DateUtils.SECOND_IN_MILLIS * 10;
//
//     private Context targetContext;
//
//     UiDevice device;
//
//     @Rule
//     public GrantPermissionRule permissionRule = GrantPermissionRule.grant(Manifest.permission.WRITE_EXTERNAL_STORAGE);
//
//     @Rule
//     public TestRule screenshotOnFailureRule = new TestWatcher() {
//         @Override
//         protected void failed(Throwable e, Description description) {
//             // On error take a screenshot so that we can debug it easily
//             Screengrab.screenshot("FAILURE-" + getScreenshotName(description));
//         }
//
//         private String getScreenshotName(Description description) {
//             return description.getClassName().replace(".", "-")
//                     + "_"
//                     + description.getMethodName().replace(".", "-");
//         }
//     };
//
//     @Before
//     public void setUpScreenshots() {
//         Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
//         targetContext = instrumentation.getTargetContext();
//         device = UiDevice.getInstance(instrumentation);
//
//         // Use this to switch between default strategy and HostScreencap strategy
//         Screengrab.setDefaultScreenshotStrategy(new UiAutomatorScreenshotStrategy());
//     }
//
//     String getString(@StringRes int resourceId) {
//         return targetContext.getString(resourceId).trim();
//     }
//
//     String getString(@StringRes int resourceId, Object... formatArgs) {
//         return targetContext.getString(resourceId, formatArgs).trim();
//     }
//
//     public void takeScreenshotsAfterWait(String filename, int waitingTime) throws InterruptedException {
//         Thread.sleep(waitingTime);
//         Screengrab.screenshot(filename);
//     }
// }
