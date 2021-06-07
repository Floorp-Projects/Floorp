package org.mozilla.fenix.ui.robots

import android.widget.RelativeLayout
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingResourceTimeoutException
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.CoreMatchers.instanceOf
import org.hamcrest.CoreMatchers.not
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.IdlingResourceHelper.registerAddonInstallingIdlingResource
import org.mozilla.fenix.helpers.IdlingResourceHelper.unregisterAddonInstallingIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the Addons Management Settings.
 */

class SettingsSubMenuAddonsManagerRobot {
    fun verifyAddonPrompt(addonName: String) = assertAddonPrompt(addonName)

    fun clickInstallAddon(addonName: String) = selectInstallAddon(addonName)

    fun verifyDownloadAddonPrompt(
        addonName: String,
        activityTestRule: ActivityTestRule<HomeActivity>
    ) {
        try {
            assertDownloadingAddonPrompt(addonName, activityTestRule)
        } catch (e: IdlingResourceTimeoutException) {
            if (mDevice.findObject(UiSelector().text("Failed to install $addonName")).exists()) {
                clickInstallAddon(addonName)
                acceptInstallAddon()
                assertDownloadingAddonPrompt(addonName, activityTestRule)
            }
        }
    }

    fun cancelInstallAddon() = cancelInstall()
    fun acceptInstallAddon() = allowInstall()
    fun verifyAddonsItems() = assertAddonsItems()
    fun verifyAddonCanBeInstalled(addonName: String) = assertAddonCanBeInstalled(addonName)

    class Transition {
        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            fun goBackButton() = onView(allOf(withContentDescription("Navigate up")))
            goBackButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openDetailedMenuForAddon(
            addonName: String,
            interact: SettingsSubMenuAddonsManagerAddonDetailedMenuRobot.() -> Unit
        ): SettingsSubMenuAddonsManagerAddonDetailedMenuRobot.Transition {
            addonName.chars()

            onView(
                allOf(
                    withId(R.id.add_on_item),
                    hasDescendant(
                        allOf(
                            withId(R.id.add_on_name),
                            withText(addonName)
                        )
                    )
                )
            ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
                .perform(click())

            SettingsSubMenuAddonsManagerAddonDetailedMenuRobot().interact()
            return SettingsSubMenuAddonsManagerAddonDetailedMenuRobot.Transition()
        }
    }

    private fun installButtonForAddon(addonName: String) =
        onView(
            allOf(
                withContentDescription(R.string.mozac_feature_addons_install_addon_content_description),
                isDescendantOfA(withId(R.id.add_on_item)),
                hasSibling(hasDescendant(withText(addonName)))
            )
        )

    private fun selectInstallAddon(addonName: String) {
        mDevice.waitNotNull(
            Until.findObject(By.textContains(addonName)),
            waitingTime
        )

        installButtonForAddon(addonName)
            .check(matches(isCompletelyDisplayed()))
            .perform(click())
    }

    private fun assertAddonIsEnabled(addonName: String) {
        installButtonForAddon(addonName)
            .check(matches(not(isCompletelyDisplayed())))
    }

    private fun assertAddonPrompt(addonName: String) {
        onView(allOf(withId(R.id.title), withText("Add $addonName?")))
            .check(matches(isCompletelyDisplayed()))

        onView(
            allOf(
                withId(R.id.permissions),
                withText(containsString("It requires your permission to:"))
            )
        )
            .check(matches(isCompletelyDisplayed()))

        onView(allOf(withId(R.id.allow_button), withText("Add")))
            .check(matches(isCompletelyDisplayed()))

        onView(allOf(withId(R.id.deny_button), withText("Cancel")))
            .check(matches(isCompletelyDisplayed()))
    }

    private fun assertDownloadingAddonPrompt(
        addonName: String,
        activityTestRule: ActivityTestRule<HomeActivity>
    ) {
        registerAddonInstallingIdlingResource(activityTestRule)

        onView(
            allOf(
                withText("Okay, Got it"),
                withParent(instanceOf(RelativeLayout::class.java)),
                hasSibling(withText("$addonName has been added to $appName")),
                hasSibling(withText("Open it in the menu")),
                hasSibling(withText("Allow in private browsing"))
            )
        )
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
            .perform(click())

        unregisterAddonInstallingIdlingResource(activityTestRule)

        TestHelper.scrollToElementByText(addonName)

        assertAddonIsInstalled(addonName)
    }

    private fun assertAddonIsInstalled(addonName: String) {
        onView(
            allOf(
                withId(R.id.add_button),
                isDescendantOfA(withId(R.id.add_on_item)),
                hasSibling(hasDescendant(withText(addonName)))
            )
        ).check(matches(withEffectiveVisibility(Visibility.GONE)))
    }

    private fun cancelInstall() {
        onView(allOf(withId(R.id.deny_button), withText("Cancel")))
            .check(matches(isCompletelyDisplayed()))
            .perform(click())
    }

    private fun allowInstall() {
        onView(allOf(withId(R.id.allow_button), withText("Add")))
            .check(matches(isCompletelyDisplayed()))
            .perform(click())
    }

    private fun assertAddonsItems() {
        assertRecommendedTitleDisplayed()
        assertAddons()
    }

    private fun assertRecommendedTitleDisplayed() {
        onView(allOf(withId(R.id.title), withText("Recommended")))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    private fun assertEnabledTitleDisplayed() {
        onView(withText("Enabled"))
            .check(matches(isCompletelyDisplayed()))
    }

    private fun assertAddons() {
        assertAddonUblock()
    }

    private fun assertAddonUblock() {
        onView(
            allOf(
                isAssignableFrom(RelativeLayout::class.java),
                withId(R.id.add_on_item),
                hasDescendant(allOf(withId(R.id.add_on_icon), isCompletelyDisplayed())),
                hasDescendant(
                    allOf(
                        withId(R.id.details_container),
                        hasDescendant(withText("uBlock Origin")),
                        hasDescendant(withText("Finally, an efficient wide-spectrum content blocker. Easy on CPU and memory.")),
                        hasDescendant(withId(R.id.rating)),
                        hasDescendant(withId(R.id.users_count))
                    )
                ),
                hasDescendant(withId(R.id.add_button))
            )
        ).check(matches(isCompletelyDisplayed()))
    }

    private fun assertAddonCanBeInstalled(addonName: String) {
        device.waitNotNull(Until.findObject(By.text(addonName)), waitingTime)

        onView(
            allOf(
                withId(R.id.add_button),
                hasSibling(
                    hasDescendant(
                        allOf(
                            withId(R.id.add_on_name),
                            withText(addonName)
                        )
                    )
                )
            )
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
}
