/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withHint
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertItemIsEnabledAndVisible
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.checkedItemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithClassNameAndIndex
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the Privacy Settings > saved logins sub menu
 */

class SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot {
    fun verifySecurityPromptForLogins() = assertSavedLoginsView()

    fun verifyEmptySavedLoginsListView() {
        onView(withText(getStringResource(R.string.preferences_passwords_saved_logins_description_empty_text)))
            .check(matches(isDisplayed()))

        onView(withText(R.string.preferences_passwords_saved_logins_description_empty_learn_more_link))
            .check(matches(isDisplayed()))

        onView(withText(R.string.preferences_logins_add_login))
            .check(matches(isDisplayed()))
    }

    fun verifySavedLoginsAfterSync() {
        mDevice.waitNotNull(
            Until.findObjects(By.text("https://accounts.google.com")),
            TestAssetHelper.waitingTime,
        )
        assertSavedLoginAppears()
    }

    fun tapSetupLater() = onView(withText("Later")).perform(ViewActions.click())

    fun clickAddLoginButton() =
        itemContainingText(getStringResource(R.string.preferences_logins_add_login)).click()

    fun verifyAddNewLoginView() {
        assertUIObjectExists(
            siteHeader,
            siteTextInput,
            usernameHeader,
            usernameTextInput,
            passwordHeader,
            passwordTextInput,
            siteDescription,
        )
        siteTextInputHint.check(matches(withHint(R.string.add_login_hostname_hint_text)))
    }

    fun enterSiteCredential(website: String) = siteTextInput.setText(website)

    fun verifyHostnameErrorMessage() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.add_login_hostname_invalid_text_2)))

    fun verifyPasswordErrorMessage() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.saved_login_password_required)))

    fun verifyPasswordClearButtonEnabled() =
        assertItemIsEnabledAndVisible(itemWithResId("$packageName:id/clearPasswordTextButton"))

    fun verifyHostnameClearButtonEnabled() =
        assertItemIsEnabledAndVisible(itemWithResId("$packageName:id/clearHostnameTextButton"))

    fun clickSearchLoginButton() = itemWithResId("$packageName:id/search").click()

    fun clickSavedLoginsChevronIcon() = itemWithResId("$packageName:id/toolbar_chevron_icon").click()

    fun verifyLoginsSortingOptions() {
        assertUIObjectExists(itemContainingText(getStringResource(R.string.saved_logins_sort_strategy_alphabetically)))
        assertUIObjectExists(itemContainingText(getStringResource(R.string.saved_logins_sort_strategy_last_used)))
    }

    fun clickLastUsedSortingOption() =
        itemContainingText(getStringResource(R.string.saved_logins_sort_strategy_last_used)).click()

    fun verifySortedLogin(position: Int, loginTitle: String) =
        assertUIObjectExists(
            itemWithClassNameAndIndex(className = "android.view.ViewGroup", index = position)
                .getChild(
                    UiSelector()
                        .resourceId("$packageName:id/webAddressView")
                        .textContains(loginTitle),
                ),
        )

    fun searchLogin(searchTerm: String) =
        itemContainingText(getStringResource(R.string.preferences_passwords_saved_logins_search)).setText(searchTerm)

    fun verifySavedLoginsSectionUsername(username: String) =
        mDevice.waitNotNull(Until.findObjects(By.text(username)))

    fun verifyLoginItemUsername(username: String) = assertUIObjectExists(itemContainingText(username))

    fun verifyNotSavedLoginFromPrompt() = onView(withText("test@example.com"))
        .check(ViewAssertions.doesNotExist())

    fun verifyLocalhostExceptionAdded() = onView(withText(containsString("localhost")))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

    fun viewSavedLoginDetails(loginUserName: String) = onView(withText(loginUserName)).click()

    fun clickThreeDotButton(activityTestRule: HomeActivityIntentTestRule) =
        openActionBarOverflowOrOptionsMenu(activityTestRule.activity)

    fun clickEditLoginButton() = itemContainingText("Edit").click()

    fun clickDeleteLoginButton() = itemContainingText("Delete").click()

    fun verifyLoginDeletionPrompt() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.login_deletion_confirmation)))

    fun clickConfirmDeleteLogin() =
        onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog()).click()

    fun clickCancelDeleteLogin() =
        onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog()).click()

    fun setNewUserName(userName: String) = usernameTextInput.setText(userName)

    fun clickClearUserNameButton() = itemWithResId("$packageName:id/clearUsernameTextButton").click()

    fun setNewPassword(password: String) = passwordTextInput.setText(password)

    fun clickClearPasswordButton() = itemWithResId("$packageName:id/clearPasswordTextButton").click()

    fun saveEditedLogin() = itemWithResId("$packageName:id/save_login_button").click()

    fun verifySaveLoginButtonIsEnabled(isEnabled: Boolean) =
        assertUIObjectExists(
            checkedItemWithResId("$packageName:id/save_login_button", isChecked = true),
            exists = isEnabled,
        )

    fun revealPassword() = onView(withId(R.id.revealPasswordButton)).click()

    fun verifyPasswordSaved(password: String) =
        onView(withId(R.id.passwordText)).check(matches(withText(password)))

    fun verifyUserNameRequiredErrorMessage() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.saved_login_username_required)))

    fun verifyPasswordRequiredErrorMessage() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.saved_login_password_required)))

    fun clickGoBackButton() = goBackButton().click()

    fun clickCopyUserNameButton() =
        itemWithResId("$packageName:id/copyUsername").also {
            it.waitForExists(waitingTime)
            it.click()
        }

    fun clickCopyPasswordButton() =
        itemWithResId("$packageName:id/copyPassword").also {
            it.waitForExists(waitingTime)
            it.click()
        }

    class Transition {
        fun goBack(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsSubMenuLoginsAndPasswordRobot().interact()
            return SettingsSubMenuLoginsAndPasswordRobot.Transition()
        }

        fun goBackToSavedLogins(interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            goBackButton().perform(ViewActions.click())

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun goToSavedWebsite(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            openWebsiteButton.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun assertSavedLoginsView() =
    onView(withText("Secure your logins and passwords"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSavedLoginAppears() =
    onView(withText("https://accounts.google.com"))
        .check(matches(isDisplayed()))

private val openWebsiteButton = onView(withId(R.id.openWebAddress))

private val siteHeader = itemWithResId("$packageName:id/hostnameHeaderText")
private val siteTextInput = itemWithResId("$packageName:id/hostnameText")
private val siteDescription = itemContainingText(getStringResource(R.string.add_login_hostname_invalid_text_3))
private val siteTextInputHint = onView(withId(R.id.hostnameText))
private val usernameHeader = itemWithResId("$packageName:id/usernameHeader")
private val usernameTextInput = itemWithResId("$packageName:id/usernameText")
private val passwordHeader = itemWithResId("$packageName:id/passwordHeader")
private val passwordTextInput = itemWithResId("$packageName:id/passwordText")
