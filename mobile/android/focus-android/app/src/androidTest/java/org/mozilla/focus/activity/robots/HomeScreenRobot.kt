/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.waitingTimeShort

class HomeScreenRobot {

    fun verifyEmptySearchBar() {
        editURLBar.waitForExists(waitingTime)
        assertTrue(editURLBar.text.equals(getStringResource(R.string.urlbar_hint)))
    }

    fun skipFirstRun() = onView(withId(R.id.skip)).perform(click())

    fun closeOnboarding() = onboardingCloseButton.clickAndWaitForNewWindow(waitingTime)

    fun verifyOnboardingFirstSlide() = assertTrue(firstSlideTitle.waitForExists(waitingTime))

    fun verifyOnboardingSecondSlide() = assertTrue(secondSlideTitle.waitForExists(waitingTime))

    fun verifyOnboardingThirdSlide() = assertTrue(thirdSlideTitle.waitForExists(waitingTime))

    fun verifyOnboardingLastSlide() = assertTrue(lastSlide.waitForExists(waitingTime))

    fun clickOnboardingNextButton() = nextButton.clickAndWaitForNewWindow(waitingTimeShort)

    fun clickOnboardingFinishButton() = finishButton.clickAndWaitForNewWindow(waitingTimeShort)

    fun verifyPageShortcutExists(title: String) {
        assertTrue(
            topSitesList
                .getChild(UiSelector().textContains(title))
                .waitForExists(waitingTime),
        )
    }

    fun longTapPageShortcut(title: String) {
        mDevice.findObject(By.text(title)).click(4000)
    }

    fun clickRenameShortcut() {
        mDevice.findObject(UiSelector().text("Rename"))
            .also {
                it.waitForExists(waitingTimeShort)
                it.click()
            }
    }

    fun renameShortcutAndSave(newTitle: String) {
        val titleTextField = mDevice.findObject(UiSelector().className("android.widget.EditText"))
        val okButton = mDevice.findObject(UiSelector().textContains("ok"))

        titleTextField.clearTextField()
        titleTextField.setText(newTitle)
        okButton.click()
        // the dialog is not always dismissed on first click of the Ok button (not manually reproducible)
        if (!mDevice.findObject(UiSelector().text("Rename")).waitUntilGone(waitingTimeShort)) {
            okButton.click()
        }
    }

    fun verifyFirstOnboardingScreenItems() {
        assertTrue(onboardingCloseButton.waitForExists(waitingTime))
        assertTrue(onboardingLogo.waitForExists(waitingTime))
        assertTrue(onboardingFirstScreenTitle.waitForExists(waitingTime))
        assertTrue(onboardingFirstScreenSubtitle.waitForExists(waitingTime))
        assertTrue(onboardingGetStartedButton.waitForExists(waitingTime))
    }

    fun verifySecondOnboardingScreenItems() {
        assertTrue(onboardingCloseButton.waitForExists(waitingTime))
        assertTrue(onboardingLogo.waitForExists(waitingTime))
        assertTrue(onboardingSecondScreenTitle.waitForExists(waitingTime))
        assertTrue(onboardingSecondScreenFirstSubtitle.waitForExists(waitingTime))
        assertTrue(onboardingSecondScreenSecondSubtitle.waitForExists(waitingTime))
        assertTrue(onboardingSetAsDefaultBrowserButton.waitForExists(waitingTime))
        assertTrue(onboardingSkipButton.waitForExists(waitingTime))
    }

    fun clickGetStartedButton() {
        onboardingGetStartedButton
            .also { it.waitForExists(waitingTime) }
            .also { it.clickAndWaitForNewWindow(waitingTime) }
    }

    class Transition {
        fun openMainMenu(interact: ThreeDotMainMenuRobot.() -> Unit): ThreeDotMainMenuRobot.Transition {
            editURLBar.waitForExists(waitingTime)
            mainMenu
                .check(matches(isDisplayed()))
                .perform(click())

            ThreeDotMainMenuRobot().interact()
            return ThreeDotMainMenuRobot.Transition()
        }

        fun clickPageShortcut(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTimeShort)
            mDevice.findObject(UiSelector().text(title)).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openSearchBar(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            editURLBar.waitForExists(waitingTime)
            editURLBar.click()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

private val editURLBar =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view"),
    )

private val mainMenu = onView(withId(R.id.menuView))

/********* First Run Locators  */
private val firstSlideTitle =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.firstrun_defaultbrowser_title)))

private val secondSlideTitle =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.firstrun_search_title)))

private val thirdSlideTitle =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.firstrun_shortcut_title)))

private val lastSlide =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.firstrun_privacy_title)))

private val nextButton = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/next")
        .enabled(true),
)

private val finishButton = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/finish")
        .enabled(true),
)

private val topSitesList = mDevice.findObject(UiSelector().resourceId("$packageName:id/topSites"))

/** New onboarding elements **/

private val onboardingCloseButton =
    mDevice.findObject(
        UiSelector()
            .descriptionContains(getStringResource(R.string.onboarding_close_button_content_description)),
    )

private val onboardingLogo =
    mDevice.findObject(
        UiSelector()
            .className("android.widget.ImageView")
            .descriptionContains(appName),
    )

private val onboardingFirstScreenTitle =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.onboarding_first_screen_title)),
    )

private val onboardingSecondScreenTitle =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.onboarding_short_app_name) + " isn’t like other browsers"),
    )

private val onboardingFirstScreenSubtitle =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.onboarding_first_screen_subtitle)),
    )

private val onboardingSecondScreenFirstSubtitle =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.onboarding_second_screen_subtitle_one)),
    )

private val onboardingSecondScreenSecondSubtitle =
    mDevice.findObject(
        UiSelector()
            .textContains(
                "Make " + getStringResource(R.string.onboarding_short_app_name) +
                    " your default to protect your data with every link you open.",
            ),
    )

private val onboardingGetStartedButton =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.onboarding_first_screen_button_text)),
    )

private val onboardingSetAsDefaultBrowserButton =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.onboarding_second_screen_default_browser_button_text)),
    )

private val onboardingSkipButton =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.onboarding_second_screen_skip_button_text)),
    )
