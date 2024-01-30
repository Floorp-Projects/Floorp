/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.content.Context
import android.net.Uri
import android.util.Log
import android.view.View
import androidx.test.core.app.launchActivity
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import mozilla.components.support.ktx.android.content.appName
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.Matcher
import org.junit.Assert
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.ui.robots.clickPageObject

object TestHelper {

    val appContext: Context = InstrumentationRegistry.getInstrumentation().targetContext
    val appName = appContext.appName
    var mDevice: UiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    val packageName: String = appContext.packageName

    fun scrollToElementByText(text: String): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        Log.i(TAG, "scrollToElementByText: Waiting for app view")
        appView.waitForExists(waitingTime)
        appView.scrollTextIntoView(text)
        Log.i(TAG, "scrollToElementByText: Scrolled to element with text: $text")
        return appView
    }

    fun longTapSelectItem(url: Uri) {
        mDevice.waitNotNull(
            Until.findObject(By.text(url.toString())),
            waitingTime,
        )
        onView(
            allOf(
                withId(R.id.url),
                withText(url.toString()),
            ),
        ).perform(longClick())
    }

    fun restartApp(activity: HomeActivityIntentTestRule) {
        with(activity) {
            updateCachedSettings()
            finishActivity()
            mDevice.waitForIdle()
            launchActivity(null)
        }
    }

    fun closeApp(activity: HomeActivityIntentTestRule) =
        activity.activity.finishAndRemoveTask()

    fun relaunchCleanApp(activity: HomeActivityIntentTestRule) {
        closeApp(activity)
        Intents.release()
        activity.launchActivity(null)
    }

    fun waitUntilObjectIsFound(resourceName: String) {
        mDevice.waitNotNull(
            Until.findObjects(By.res(resourceName)),
            waitingTime,
        )
    }

    fun clickSnackbarButton(expectedText: String) =
        clickPageObject(itemWithResIdAndText("$packageName:id/snackbar_btn", expectedText))

    fun waitUntilSnackbarGone() {
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/snackbar_layout"),
        ).waitUntilGone(waitingTime)
    }

    fun verifySnackBarText(expectedText: String) = assertUIObjectExists(itemContainingText(expectedText))

    fun verifyUrl(urlSubstring: String, resourceName: String, resId: Int) {
        waitUntilObjectIsFound(resourceName)
        mDevice.findObject(UiSelector().text(urlSubstring)).waitForExists(waitingTime)
        onView(withId(resId)).check(ViewAssertions.matches(withText(CoreMatchers.containsString(urlSubstring))))
    }

    // exit from Menus to home screen or browser
    fun exitMenu() {
        val toolbar =
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
        while (!toolbar.waitForExists(waitingTimeShort)) {
            mDevice.pressBack()
            Log.i(TAG, "exitMenu: Exiting app settings menus using device back button")
        }
    }

    fun UiDevice.waitForObjects(obj: UiObject, waitingTime: Long = TestAssetHelper.waitingTime) {
        this.waitForIdle()
        Assert.assertNotNull(obj.waitForExists(waitingTime))
    }

    fun hasCousin(matcher: Matcher<View>): Matcher<View> {
        return withParent(
            hasSibling(
                withChild(
                    matcher,
                ),
            ),
        )
    }

    fun verifyLightThemeApplied(expected: Boolean) =
        assertFalse("Light theme not selected", expected)

    fun verifyDarkThemeApplied(expected: Boolean) = assertTrue("Dark theme not selected", expected)
}
