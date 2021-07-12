/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.closeSoftKeyboard
import androidx.test.espresso.action.ViewActions.swipeDown
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withSubstring
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.startsWith
import org.hamcrest.Matchers
import org.junit.Assert.assertEquals
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.SessionLoadedIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the search fragment.
 */
class SearchRobot {
    fun verifySearchView() = assertSearchView()
    fun verifyBrowserToolbar() = assertBrowserToolbarEditView()
    fun verifyScanButton() = assertScanButton()
    fun verifySearchEngineButton() = assertSearchEngineButton()
    fun verifySearchWithText() = assertSearchWithText()
    fun verifySearchEngineResults(searchEngineName: String) =
        assertSearchEngineResults(searchEngineName)

    fun verifySearchEngineURL(searchEngineName: String) = assertSearchEngineURL(searchEngineName)
    fun verifySearchSettings() = assertSearchSettings()
    fun verifySearchBarEmpty() = assertSearchBarEmpty()

    fun verifyKeyboardVisibility() = assertKeyboardVisibility(isExpectedToBeVisible = true)
    fun verifySearchEngineList() = assertSearchEngineList()
    fun verifySearchEngineIcon(expectedText: String) {
        onView(withContentDescription(expectedText))
    }
    fun verifyDefaultSearchEngine(expectedText: String) = assertDefaultSearchEngine(expectedText)

    fun verifyEnginesListShortcutContains(searchEngineName: String) = assertEngineListShortcutContains(searchEngineName)

    fun changeDefaultSearchEngine(searchEngineName: String) =
        selectDefaultSearchEngine(searchEngineName)

    fun clickSearchEngineShortcutButton() {
        val searchEnginesShortcutButton = mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/search_engines_shortcut_button")
        )
        searchEnginesShortcutButton.waitForExists(waitingTime)
        searchEnginesShortcutButton.click()
    }

    fun clickScanButton() {
        scanButton().perform(click())
    }

    fun clickDenyPermission() {
        denyPermissionButton().click()
    }

    fun clickAllowPermission() {
        allowPermissionButton().click()
    }

    fun typeSearch(searchTerm: String) {
        browserToolbarEditView().perform(typeText(searchTerm))
    }

    fun clickSearchEngineButton(searchEngineName: String) {
        searchEngineButton(searchEngineName).perform(click())
    }

    fun clickSearchEngineResult(searchEngineName: String) {
        mDevice.waitNotNull(
            Until.findObjects(By.text(searchEngineName)),
            TestAssetHelper.waitingTime
        )
        awesomeBar().perform(
            RecyclerViewActions.actionOnItemAtPosition<RecyclerView.ViewHolder>(
                0,
                click()
            )
        )
    }

    fun scrollToSearchEngineSettings() {
        // Soft keyboard is visible on screen on view access; hide it
        onView(allOf(withId(R.id.search_wrapper))).perform(
            closeSoftKeyboard()
        )
        onView(allOf(withId(R.id.awesome_bar))).perform(ViewActions.swipeUp())
    }

    fun clickSearchEngineSettings() {
        onView(withText("Search engine settings")).perform(click())
    }

    fun clickClearButton() {
        clearButton().perform(click())
    }

    fun longClickToolbar() {
        mDevice.waitForWindowUpdate(packageName, waitingTime)
        mDevice.findObject(UiSelector().resourceId("$packageName:id/awesomeBar"))
            .waitForExists(waitingTime)
        mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
            .waitForExists(waitingTime)
        val toolbar = mDevice.findObject(By.res("$packageName:id/toolbar"))
        toolbar.click(LONG_CLICK_DURATION)
    }

    fun clickPasteText() {
        mDevice.findObject(UiSelector().textContains("Paste")).waitForExists(waitingTime)
        val pasteText = mDevice.findObject(By.textContains("Paste"))
        pasteText.click()
    }

    fun verifyPastedToolbarText(expectedText: String) = assertPastedToolbarText(expectedText)

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource

        fun dismissSearchBar(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.waitForIdle()
            mDevice.pressBack()
            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitForIdle()
            browserToolbarEditView().perform(typeText("mozilla\n"))

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun submitQuery(query: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            sessionLoadedIdlingResource = SessionLoadedIdlingResource()
            mDevice.waitForIdle()
            browserToolbarEditView().perform(typeText(query + "\n"))

            runWithIdleRes(sessionLoadedIdlingResource) {
                onView(ViewMatchers.withResourceName("browserLayout"))
                    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun goToSearchEngine(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }
    }
}

private fun awesomeBar() = onView(withId(R.id.awesome_bar))

private fun browserToolbarEditView() =
    onView(Matchers.allOf(withId(R.id.mozac_browser_toolbar_edit_url_view)))

private fun searchEngineButton(searchEngineName: String): ViewInteraction {
    mDevice.waitNotNull(Until.findObject(By.text(searchEngineName)), TestAssetHelper.waitingTime)
    return onView(Matchers.allOf(withText(searchEngineName)))
}

private fun denyPermissionButton(): UiObject {
    mDevice.waitNotNull(Until.findObjects(By.text("Deny")), TestAssetHelper.waitingTime)
    return mDevice.findObject(UiSelector().text("Deny"))
}

private fun allowPermissionButton(): UiObject {
    mDevice.waitNotNull(Until.findObjects(By.text("Allow")), TestAssetHelper.waitingTime)
    return mDevice.findObject(UiSelector().text("Allow"))
}

private fun scanButton(): ViewInteraction {
    mDevice.waitNotNull(Until.findObject(By.res("org.mozilla.fenix.debug:id/search_scan_button")), TestAssetHelper.waitingTime)
    return onView(allOf(withId(R.id.search_scan_button)))
}

private fun clearButton() = onView(withId(R.id.mozac_browser_toolbar_clear_view))

private fun searchWrapper() = onView(withId(R.id.search_wrapper))

private fun assertSearchEngineURL(searchEngineName: String) {
    mDevice.waitNotNull(
        Until.findObject(By.textContains("${searchEngineName.lowercase()}.com/?q=mozilla")),
        TestAssetHelper.waitingTime
    )
    onView(allOf(withText(startsWith("${searchEngineName.lowercase()}.com"))))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertSearchEngineResults(searchEngineName: String) {
    val count =
        mDevice.wait(Until.findObjects(By.text((searchEngineName))), TestAssetHelper.waitingTime)
    assert(count.size > 1)
}

private fun assertSearchView() {
    onView(withId(R.id.search_wrapper)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertBrowserToolbarEditView() =
    onView(Matchers.allOf(withId(R.id.mozac_browser_toolbar_edit_url_view)))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertScanButton() =
    onView(allOf(withText("Scan")))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSearchEngineButton() =
    onView(withId(R.id.search_engines_shortcut_button))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertSearchWithText() =
    onView(allOf(withText("THIS TIME, SEARCH WITH:")))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSearchSettings() =
    onView(allOf(withText("Default search engine")))
        .check(matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSearchBarEmpty() = browserToolbarEditView().check(matches(withText("")))

fun searchScreen(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
    SearchRobot().interact()
    return SearchRobot.Transition()
}

private fun assertKeyboardVisibility(isExpectedToBeVisible: Boolean) = {
    mDevice.waitNotNull(
        Until.findObject(
            By.text("Search Engine")
        ),
        waitingTime
    )
    assertEquals(
        isExpectedToBeVisible,
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
            .executeShellCommand("dumpsys input_method | grep mInputShown")
            .contains("mInputShown=true")
    )
}

private fun assertSearchEngineList() {
    onView(withId(R.id.mozac_browser_toolbar_edit_icon)).click()
    onView(withText("Google"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    onView(withText("Amazon.com"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    onView(withText("Bing"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    onView(withText("DuckDuckGo"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    onView(withText("Wikipedia"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEngineListShortcutContains(searchEngineName: String) {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/awesome_bar"))
        .waitForExists(waitingTime)

    onView(withId(R.id.awesome_bar))
        .perform(swipeDown())
        .check(matches(hasDescendant(withText(searchEngineName))))
}

private fun selectDefaultSearchEngine(searchEngine: String) {
    onView(withId(R.id.mozac_browser_toolbar_edit_icon)).click()
    onView(withText(searchEngine))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())
}

private fun assertDefaultSearchEngine(expectedText: String) {
    onView(allOf(withId(R.id.mozac_browser_toolbar_edit_icon), withContentDescription(expectedText)))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertPastedToolbarText(expectedText: String) {
    mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
        .waitForExists(waitingTime)
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_url_view"))
        .waitForExists(waitingTime)
    onView(
        allOf(
            withSubstring(expectedText),
            withId(R.id.mozac_browser_toolbar_edit_url_view)
        )
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun goBackButton() = onView(allOf(withContentDescription("Navigate up")))
