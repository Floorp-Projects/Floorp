/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.syncintegration

import android.os.SystemClock.sleep
import android.widget.Button
import android.widget.EditText
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.action.ViewActions.pressImeActionButton
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText

import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R

@Suppress("RECEIVER_NULLABILITY_MISMATCH_BASED_ON_JAVA_ANNOTATIONS")
class SyncIntegrationTest {
    val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    // History item Desktop -> Fenix
    @Test
    fun checkHistoryFromDesktopTest() {
        signInFxSync()
        tapReturnToPreviousApp()
        homeScreen {
        }.openThreeDotMenu {}
        libraryButton()
        historyButton()
        historyAfterSyncIsShown()
    }
    /* These tests will be running in the future
    // once the test above runs successfully and
    // the environment is stable

    // Bookmark item Desktop -> Fenix
    @Test
    fun checkBookmarkFromDesktopTest() {
        signInFxSync()
        tapReturnToPreviousApp()
        sleep(5000)
        homeScreen {
        }.openThreeDotMenu {}
        libraryButton()
        bookmarkButton()
        bookmarkAfterSyncIsShown()
    }

    // History item Fenix -> Desktop
    @Test
    fun checkBookmarkFromDeviceTest() {
        tapInToolBar()
        typeInToolBar()
        seeBookmark()
        mDevice.pressBack()
        signInFxSync()
    }

    // Bookmark item Fenix -> Desktop
    @Test
    fun checkHistoryFromDeviceTest() {
        tapInToolBar()
        typeInToolBar()
        sleep(3000)
        mDevice.pressBack()
        signInFxSync()
    }
    */

    // Useful functions for the tests
    fun typeEmail() {
        val emailInput = mDevice.findObject(UiSelector()
                .instance(0)
                .className(EditText::class.java))
        emailInput.waitForExists(1000)

        val emailAddress = javaClass.classLoader.getResource("email.txt").readText()
        emailInput.setText(emailAddress)
    }

    fun tapOnContinueButton() {
        val continueButton = mDevice.findObject(By.res("submit-btn"))
        continueButton.clickAndWait(Until.newWindow(), 50000)
    }

    fun typePassowrd() {
        val passwordInput = mDevice.findObject(UiSelector()
                .instance(0)
                .className(EditText::class.java))

        val passwordValue = javaClass.classLoader.getResource("password.txt").readText()
        passwordInput.setText(passwordValue)
    }

    fun tapOnSygIn() {
        mDevice.pressEnter()
        mDevice.wait(Until.findObjects(By.text("Sign in")), 3000)
        val signInButton = mDevice.findObject(UiSelector()
                .instance(0)
                .className(Button::class.java))
        signInButton.waitForExists(10000)
        signInButton.click()
    }

    fun typeInToolBar() {
        awesomeBar().perform(replaceText("example.com"),
                pressImeActionButton())
    }

    fun historyAfterSyncIsShown() {
        val historyEntry = mDevice.findObject(By.text("http://www.example.com/"))
        historyEntry.isEnabled()
    }

    fun bookmarkAfterSyncIsShown() {
        val bookmarkyEntry = mDevice.findObject(By.text("Example Domain"))
        bookmarkyEntry.isEnabled()
    }

    fun seeBookmark() {
        mDevice.wait(Until.findObjects(By.text("Bookmark")), 3000)
        val bookmarkButton = mDevice.findObject(By.text("Bookmark"))
        bookmarkButton.click()
    }

    fun tapReturnToPreviousApp() {
        mDevice.wait(Until.findObjects(By.text("Connected")), 2000)

        val settingsLabel = mDevice.wait(Until.findObject(By.text("Settings")), 20000)
        settingsLabel.isClickable()

        mDevice.pressBack()
    }

    fun signInFxSync() {
        homeScreen {
        }.openThreeDotMenu {
            verifySettingsButton()
        }.openSettings {}
        settingsAccount()
        useEmailInsteadButton()

        typeEmail()
        tapOnContinueButton()
        typePassowrd()
        sleep(3000)
        tapOnSygIn()
    }
}

fun settingsAccount() = onView(allOf(withText("Turn on Sync"))).perform(click())
fun tapInToolBar() = onView(withId(org.mozilla.fenix.R.id.toolbar_wrapper))
fun awesomeBar() = onView(withId(org.mozilla.fenix.R.id.mozac_browser_toolbar_edit_url_view))
fun libraryButton() = onView(allOf(withText(R.string.browser_menu_your_library))).perform(click())
fun historyButton() = onView(allOf(withText("History"))).perform(click())
fun bookmarkButton() = onView(allOf(withText("Bookmarks"))).perform(click())
fun useEmailInsteadButton() = onView(withId(R.id.signInEmailButton)).perform(click())
