/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.startsWith
import org.hamcrest.Matchers
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper

/**
 * Implementation of Robot Pattern for the search fragment.
 */
class SearchRobot {
    fun verifySearchView() = assertSearchView()
    fun verifyBrowserToolbar() = assertBrowserToolbarEditView()
    fun verifyScanButton() = assertScanButton()
    fun verifyShortcutsButton() = assertShortcutsButton()
    fun verifySearchWithText() = assertSearchWithText()
    fun verifyDuckDuckGoResults() = assertDuckDuckGoResults()
    fun verifyDuckDuckGoURL() = assertDuckDuckGoURL()
    fun verifySearchEngineSettings() = assertSearchEngineSettings()

    fun clickScanButton() {
        scanButton().perform(click())
    }

    fun clickDenyPermission() {
        denyPermissionButton().click()
    }

    fun clickAllowPermission() {
        allowPermissionButton().click()
    }

    fun clickShortcutsButton() {
        shortcutsButton().perform(click())
    }

    fun typeSearch() {
        browserToolbarEditView().perform(typeText("Mozilla"))
    }

    fun clickDuckDuckGoEngineButton() {
        duckDuckGoEngineButton().perform(click())
    }

    fun clickDuckDuckGoResult() {
        mDevice.wait(Until.findObjects(By.text("DuckDuckGo")), TestAssetHelper.waitingTime)
        awesomeBar().perform(RecyclerViewActions.actionOnItemAtPosition<RecyclerView.ViewHolder>(0, click()))
    }

    fun scrollToSearchEngineSettings(): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        appView.scrollTextIntoView("Search engine settings")
        return appView
    }

    fun clickSearchEngineSettings() {
        onView(withText("Search engine settings")).perform(click())
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitForIdle()
            browserToolbarEditView().perform(typeText("Mozilla\n"))

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private fun awesomeBar() = onView(withId(R.id.awesomeBar))

private fun browserToolbarEditView() = onView(Matchers.allOf(withId(R.id.mozac_browser_toolbar_edit_url_view)))

private fun duckDuckGoEngineButton(): ViewInteraction {
    mDevice.wait(Until.findObject(By.text("DuckDuckGo")), TestAssetHelper.waitingTime)
    return onView(Matchers.allOf(withText("DuckDuckGo")))
}

private fun denyPermissionButton(): UiObject {
    mDevice.wait(Until.findObjects(By.text("Deny")), TestAssetHelper.waitingTime)
    return mDevice.findObject(UiSelector().text("Deny"))
}

private fun allowPermissionButton(): UiObject {
    mDevice.wait(Until.findObjects(By.text("Allow")), TestAssetHelper.waitingTime)
    return mDevice.findObject(UiSelector().text("Allow"))
}

private fun scanButton(): ViewInteraction {
    mDevice.wait(Until.findObject(By.res("R.id.search_scan_button")), TestAssetHelper.waitingTime)
    return onView(allOf(withId(R.id.searchScanButton)))
}

private fun shortcutsButton(): ViewInteraction {
    mDevice.wait(Until.findObjects(By.res("R.id.search_shortcuts_button")), TestAssetHelper.waitingTime)
    return onView(withId(R.id.searchShortcutsButton))
}

private fun assertDuckDuckGoURL() {
    onView(allOf(withText(startsWith("https://duckduckgo.com"))))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertDuckDuckGoResults() {
    val count = mDevice.wait(Until.findObjects(By.text(("DuckDuckGo"))), TestAssetHelper.waitingTime)
    assert(count.size > 1)
}

private fun assertSearchView() {
    onView(allOf(withId(R.id.search_layout)))
}

private fun assertBrowserToolbarEditView() =
    onView(Matchers.allOf(withId(R.id.mozac_browser_toolbar_edit_url_view)))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertScanButton() =
    onView(allOf(withText("Scan")))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertShortcutsButton() =
    onView(allOf(withText("Shortcuts")))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSearchWithText() =
    onView(allOf(withText("SEARCH WITH")))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSearchEngineSettings() =
    onView(allOf(withText("Search engine")))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

fun searchScreen(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
    SearchRobot().interact()
    return SearchRobot.Transition()
}

private fun goBackButton() = onView(allOf(withContentDescription("Navigate up")))
