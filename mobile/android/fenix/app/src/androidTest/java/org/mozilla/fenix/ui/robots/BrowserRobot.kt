/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions", "TooGenericExceptionCaught")

package org.mozilla.fenix.ui.robots

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.SystemClock
import android.util.Log
import android.widget.TimePicker
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.PickerActions
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.BundleMatchers
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.SessionLoadedIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.waitForObjects
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import java.time.LocalDate

class BrowserRobot {
    private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource

    fun waitForPageToLoad() = progressBar.waitUntilGone(waitingTime)

    fun verifyCurrentPrivateSession(context: Context) {
        val selectedTab = context.components.core.store.state.selectedTab
        assertTrue("Current session is private", selectedTab?.content?.private ?: false)
    }

    fun verifyUrl(url: String) {
        sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/mozac_browser_toolbar_url_view")
                        .textContains(url.replace("http://", "")),
                ).waitForExists(waitingTime),
            )
        }
    }

    fun verifyHelpUrl() {
        verifyUrl("support.mozilla.org/")
    }

    fun verifyWhatsNewURL() {
        verifyUrl("support.mozilla.org/")
    }

    fun verifyRateOnGooglePlayURL() {
        verifyUrl("play.google.com/store/apps/details?id=org.mozilla.fenix")
    }

    /* Asserts that the text within DOM element with ID="testContent" has the given text, i.e.
    *  document.querySelector('#testContent').innerText == expectedText
    *
    */

    fun verifyPageContent(expectedText: String) {
        sessionLoadedIdlingResource = SessionLoadedIdlingResource()

        mDevice.waitNotNull(
            Until.findObject(By.res("$packageName:id/engineView")),
            waitingTime,
        )

        runWithIdleRes(sessionLoadedIdlingResource) {
            assertTrue(
                "Page didn't load or doesn't contain the expected text",
                mDevice.findObject(UiSelector().textContains(expectedText)).waitForExists(waitingTime),
            )
        }
    }

    /* Verifies the information displayed on the about:cache page */
    fun verifyNetworkCacheIsEmpty(storage: String) {
        val memorySection = mDevice.findObject(UiSelector().description(storage))

        val gridView =
            if (storage == "memory") {
                memorySection.getFromParent(
                    UiSelector()
                        .className("android.widget.GridView")
                        .index(2),
                )
            } else {
                memorySection.getFromParent(
                    UiSelector()
                        .className("android.widget.GridView")
                        .index(4),
                )
            }

        val cacheSizeInfo =
            gridView.getChild(
                UiSelector().text("Number of entries:"),
            ).getFromParent(
                UiSelector().text("0"),
            )

        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(cacheSizeInfo.waitForExists(waitingTime))
                break
            } catch (e: AssertionError) {
                browserScreen {
                }.openThreeDotMenu {
                }.refreshPage { }
            }
        }
    }

    fun verifyTabCounter(expectedText: String) {
        val counter =
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/counter_text")
                    .text(expectedText),
            )
        assertTrue(counter.waitForExists(waitingTime))
    }

    fun verifySnackBarText(expectedText: String) {
        mDevice.waitForObjects(mDevice.findObject(UiSelector().textContains(expectedText)))

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .textContains(expectedText),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyLinkContextMenuItems(containsURL: Uri) {
        mDevice.waitNotNull(
            Until.findObject(By.textContains(containsURL.toString())),
            waitingTime,
        )
        mDevice.waitNotNull(
            Until.findObject(text("Open link in new tab")),
            waitingTime,
        )
        mDevice.waitNotNull(
            Until.findObject(text("Open link in private tab")),
            waitingTime,
        )
        mDevice.waitNotNull(Until.findObject(text("Copy link")), waitingTime)
        mDevice.waitNotNull(Until.findObject(text("Share link")), waitingTime)
    }

    fun verifyLinkImageContextMenuItems(containsURL: Uri) {
        mDevice.waitNotNull(Until.findObject(By.textContains(containsURL.toString())))
        mDevice.waitNotNull(
            Until.findObject(text("Open link in new tab")),
            waitingTime,
        )
        mDevice.waitNotNull(
            Until.findObject(text("Open link in private tab")),
            waitingTime,
        )
        mDevice.waitNotNull(Until.findObject(text("Copy link")), waitingTime)
        mDevice.waitNotNull(Until.findObject(text("Share link")), waitingTime)
        mDevice.waitNotNull(
            Until.findObject(text("Open image in new tab")),
            waitingTime,
        )
        mDevice.waitNotNull(Until.findObject(text("Save image")), waitingTime)
        mDevice.waitNotNull(
            Until.findObject(text("Copy image location")),
            waitingTime,
        )
    }

    fun verifyNavURLBarHidden() = assertNavURLBarHidden()

    fun verifySecureConnectionLockIcon() = assertSecureConnectionLockIcon()

    fun verifyMenuButton() = assertMenuButton()

    fun verifyNavURLBarItems() {
        navURLBar().waitForExists(waitingTime)
        verifyMenuButton()
        verifyTabCounter("1")
        verifySearchBar()
        verifySecureConnectionLockIcon()
        verifyHomeScreenButton()
    }

    fun verifyNoLinkImageContextMenuItems(containsURL: Uri) {
        mDevice.waitNotNull(Until.findObject(By.textContains(containsURL.toString())))
        mDevice.waitNotNull(
            Until.findObject(text("Open image in new tab")),
            waitingTime,
        )
        mDevice.waitNotNull(Until.findObject(text("Save image")), waitingTime)
        mDevice.waitNotNull(
            Until.findObject(text("Copy image location")),
            waitingTime,
        )
    }

    fun verifyNotificationDotOnMainMenu() {
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/notification_dot"))
                .waitForExists(waitingTime),
        )
    }

    fun verifyHomeScreenButton() = assertHomeScreenButton()

    fun verifySearchBar() = assertSearchBar()

    fun dismissContentContextMenu(containsURL: Uri) {
        onView(withText(containsURL.toString()))
            .inRoot(isDialog())
            .check(matches(isDisplayed()))
            .perform(ViewActions.pressBack())
    }

    fun clickContextOpenLinkInNewTab() {
        mDevice.waitNotNull(
            Until.findObject(text("Open link in new tab")),
            waitingTime,
        )

        val menuOpenInNewTab = mDevice.findObject(text("Open link in new tab"))
        menuOpenInNewTab.click()
    }

    fun clickContextOpenLinkInPrivateTab() {
        mDevice.waitNotNull(
            Until.findObject(text("Open link in private tab")),
            waitingTime,
        )

        val menuOpenInPrivateTab = mDevice.findObject(text("Open link in private tab"))
        menuOpenInPrivateTab.click()
    }

    fun clickContextCopyLink() {
        mDevice.waitNotNull(Until.findObject(text("Copy link")), waitingTime)

        val menuCopyLink = mDevice.findObject(text("Copy link"))
        menuCopyLink.click()
    }

    fun clickContextShareLink(url: Uri) {
        mDevice.waitNotNull(Until.findObject(text("Share link")), waitingTime)

        val menuShareLink = mDevice.findObject(text("Share link"))
        menuShareLink.click()

        // verify share intent is launched and matched with associated passed in URL
        Intents.intended(
            allOf(
                IntentMatchers.hasAction(Intent.ACTION_CHOOSER),
                IntentMatchers.hasExtras(
                    allOf(
                        BundleMatchers.hasEntry(
                            Intent.EXTRA_INTENT,
                            allOf(
                                IntentMatchers.hasAction(Intent.ACTION_SEND),
                                IntentMatchers.hasType("text/plain"),
                                IntentMatchers.hasExtra(
                                    Intent.EXTRA_TEXT,
                                    url.toString(),
                                ),
                            ),
                        ),
                    ),
                ),
            ),
        )
    }

    fun clickContextCopyImageLocation() {
        mDevice.waitNotNull(
            Until.findObject(text("Copy image location")),
            waitingTime,
        )

        val menuCopyImageLocation = mDevice.findObject(text("Copy image location"))
        menuCopyImageLocation.click()
    }

    fun clickContextOpenImageNewTab() {
        mDevice.waitNotNull(
            Until.findObject(text("Open image in new tab")),
            waitingTime,
        )

        val menuOpenImageNewTab = mDevice.findObject(text("Open image in new tab"))
        menuOpenImageNewTab.click()
    }

    fun clickContextSaveImage() {
        mDevice.waitNotNull(Until.findObject(text("Save image")), waitingTime)

        val menuSaveImage = mDevice.findObject(text("Save image"))
        menuSaveImage.click()
    }

    fun createBookmark(url: Uri, folder: String? = null) {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(url) {
            // needs to wait for the right url to load before saving a bookmark
            verifyUrl(url.toString())
        }.openThreeDotMenu {
        }.bookmarkPage {
        }.takeIf { !folder.isNullOrBlank() }?.let {
            it.openThreeDotMenu {
            }.editBookmarkPage {
                setParentFolder(folder!!)
                saveEditBookmark()
            }
        }
    }

    fun clickLinkMatchingText(expectedText: String) =
        clickPageObject(webPageItemContainingText(expectedText))

    fun longClickLink(expectedText: String) =
        longClickPageObject(webPageItemWithText(expectedText))

    fun longClickMatchingText(expectedText: String) =
        longClickPageObject(webPageItemContainingText(expectedText))

    fun longClickAndCopyText(expectedText: String, selectAll: Boolean = false) {
        longClickPageObject(webPageItemContainingText(expectedText))

        // Click Select all from the text selection toolbar
        if (selectAll) {
            mDevice.findObject(UiSelector().textContains("Select all")).waitForExists(waitingTime)
            val selectAllText = mDevice.findObject(By.textContains("Select all"))
            selectAllText.click()
        }

        // Click Copy from the text selection toolbar
        mDevice.findObject(UiSelector().textContains("Copy")).waitForExists(waitingTime)
        val copyText = mDevice.findObject(By.textContains("Copy"))
        copyText.click()
    }

    fun longClickAndSearchText(searchButton: String, expectedText: String) {
        longClickPageObject(webPageItemContainingText(expectedText))

        // Click search from the text selection toolbar
        mDevice.findObject(UiSelector().textContains(searchButton)).waitForExists(waitingTime)
        val searchText = mDevice.findObject(By.textContains(searchButton))
        searchText.click()
    }

    fun clickSnackbarButton(expectedText: String) =
        itemWithResIdAndText("$packageName:id/snackbar_btn", expectedText)
            .also {
                it.waitForExists(waitingTime)
                it.click()
            }

    fun clickSubmitLoginButton() = clickPageObject(webPageItemWithResourceId("submit"))

    fun verifyUpdateLoginPromptIsShown() = mDevice.waitNotNull(Until.findObjects(text("Update")))

    fun saveLoginFromPrompt(optionToSaveLogin: String) {
        mDevice.waitForObjects(
            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/feature_prompt_login_fragment"),
            ),
            waitingTime,
        )
        mDevice.findObject(text(optionToSaveLogin)).click()
    }

    fun enterPassword(password: String) {
        clickPageObject(webPageItemWithResourceId("password"))
        setPageObjectText(webPageItemWithResourceId("password"), password)

        assertTrue(mDevice.findObject(UiSelector().text(password)).waitUntilGone(waitingTime))
    }

    fun clickMediaPlayerPlayButton() = clickPageObject(webPageItemWithText("Play"))

    /**
     * Get the current playback state of the currently selected tab.
     * The result may be null if there if the currently playing media tab cannot be found in [store]
     *
     * @param store [BrowserStore] from which to get data about the current tab's state.
     * @return nullable [MediaSession.PlaybackState] indicating the media playback state for the current tab.
     */
    private fun getCurrentPlaybackState(store: BrowserStore): MediaSession.PlaybackState? {
        return store.state.selectedTab?.mediaSessionState?.playbackState
    }

    /**
     * Asserts that in [waitingTime] the playback state of the current tab will be [expectedState].
     *
     * @param store [BrowserStore] from which to get data about the current tab's state.
     * @param expectedState [MediaSession.PlaybackState] the playback state that will be asserted
     * @param waitingTime maximum time the test will wait for the playback state to become [expectedState]
     * before failing the assertion.
     */
    fun assertPlaybackState(store: BrowserStore, expectedState: MediaSession.PlaybackState) {
        val startMills = SystemClock.uptimeMillis()
        var currentMills: Long = 0
        while (currentMills <= waitingTime) {
            if (expectedState == getCurrentPlaybackState(store)) return
            currentMills = SystemClock.uptimeMillis() - startMills
        }
        fail("Playback did not moved to state: $expectedState")
    }

    fun swipeNavBarRight(tabUrl: String) {
        // failing to swipe on Firebase sometimes, so it tries again
        try {
            navURLBar().swipeRight(2)
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        } catch (e: AssertionError) {
            navURLBar().swipeRight(2)
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        }
    }

    fun swipeNavBarLeft(tabUrl: String) {
        // failing to swipe on Firebase sometimes, so it tries again
        try {
            navURLBar().swipeLeft(2)
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        } catch (e: AssertionError) {
            navURLBar().swipeLeft(2)
            assertTrue(mDevice.findObject(UiSelector().text(tabUrl)).waitUntilGone(waitingTime))
        }
    }

    fun clickTabCrashedRestoreButton() {
        assertTrue(
            mDevice.findObject(UiSelector().resourceId("$packageName:id/restoreTabButton"))
                .waitForExists(waitingTime),
        )

        val tabCrashRestoreButton = mDevice.findObject(UiSelector().resourceIdMatches("$packageName:id/restoreTabButton"))
        tabCrashRestoreButton.click()
    }

    fun fillAndSubmitLoginCredentials(userName: String, password: String) {
        mDevice.waitForIdle(waitingTime)
        setPageObjectText(webPageItemWithResourceId("username"), userName)
        setPageObjectText(webPageItemWithResourceId("password"), password)
        clickPageObject(webPageItemWithResourceId("submit"))

        mDevice.waitForObjects(mDevice.findObject(UiSelector().resourceId("$packageName:id/save_confirm")), waitingTime)
    }

    fun clearUserNameLoginCredential() {
        mDevice.waitForObjects(webPageItemWithResourceId("username"))
        webPageItemWithResourceId("username").clearTextField()
        mDevice.waitForIdle(waitingTime)
    }

    fun clickUsernameTextField() =
        webPageItemWithResourceId("username").also {
            it.waitForExists(waitingTime)
            it.click()
        }

    fun clickSuggestedLoginsButton() {
        var currentTries = 0
        while (currentTries++ < 3) {
            try {
                mDevice.waitForObjects(suggestedLogins)
                suggestedLogins.click()
                mDevice.waitForObjects(suggestedLogins)
                break
            } catch (e: UiObjectNotFoundException) {
                clickPageObject(webPageItemWithResourceId("username"))
            }
        }
    }

    fun clickStreetAddressTextBox() = clickPageObject(webPageItemWithResourceId("streetAddress"))

    fun setTextForApartmentTextBox(apartment: String) =
        webPageItemWithResourceId("apartment").setText(apartment)

    fun clearAddressForm() {
        webPageItemWithResourceId("streetAddress").clearTextField()
        webPageItemWithResourceId("city").clearTextField()
        webPageItemWithResourceId("zipCode").clearTextField()
        webPageItemWithResourceId("country").clearTextField()
        webPageItemWithResourceId("telephone").clearTextField()
        webPageItemWithResourceId("email").clearTextField()
    }

    fun clickSelectAddressButton() {
        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(selectAddressButton.waitForExists(waitingTime))
                selectAddressButton.clickAndWaitForNewWindow(waitingTime)

                break
            } catch (e: AssertionError) {
                // Retrying, in case we hit https://bugzilla.mozilla.org/show_bug.cgi?id=1816869
                // This should be removed when the bug is fixed.
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    clickPageObject(webPageItemWithResourceId("city"))
                    clickPageObject(webPageItemWithResourceId("country"))
                }
            }
        }
    }

    fun verifySelectAddressButtonExists(exists: Boolean) = assertItemWithResIdExists(selectAddressButton, exists = exists)

    fun clickCreditCardNumberTextBox() = clickPageObject(webPageItemWithResourceId("cardNumber"))

    fun changeCreditCardExpiryDate(expiryDate: String) =
        webPageItemWithResourceId("expiryMonthAndYear").setText(expiryDate)

    fun clickCreditCardFormSubmitButton() =
        webPageItemWithResourceId("submit").clickAndWaitForNewWindow(waitingTime)

    fun fillAndSaveCreditCard(cardNumber: String, cardName: String, expiryMonthAndYear: String) {
        webPageItemWithResourceId("cardNumber").setText(cardNumber)
        webPageItemWithResourceId("nameOnCard").setText(cardName)
        webPageItemWithResourceId("expiryMonthAndYear").setText(expiryMonthAndYear)
        webPageItemWithResourceId("submit").clickAndWaitForNewWindow(waitingTime)
    }

    fun clickCancelCreditCardPromptButton() =
        itemWithResId("$packageName:id/save_cancel").also {
            it.waitForExists(waitingTime)
            it.click()
        }

    fun clickUpdateOrSaveCreditCardPromptButton() =
        itemWithResId("$packageName:id/save_confirm").also {
            it.waitForExists(waitingTime)
            it.click()
        }

    fun verifyUpdateOrSaveCreditCardPromptExists(exists: Boolean) =
        assertItemWithResIdAndTextExists(
            itemWithResId("$packageName:id/save_credit_card_header"),
            exists = exists,
        )

    fun clickSelectCreditCardButton() {
        selectCreditCardButton.waitForExists(waitingTime)
        selectCreditCardButton.clickAndWaitForNewWindow(waitingTime)
    }

    fun verifySelectCreditCardPromptExists(exists: Boolean) =
        assertItemWithResIdExists(selectCreditCardButton, exists = exists)

    fun clickLoginSuggestion(userName: String) {
        val loginSuggestion =
            mDevice.findObject(
                UiSelector()
                    .textContains(userName)
                    .resourceId("$packageName:id/username"),
            )

        loginSuggestion.click()
    }

    fun clickAddressSuggestion(streetName: String) {
        addressSuggestion(streetName).waitForExists(waitingTime)
        addressSuggestion(streetName).click()
    }

    fun clickCreditCardSuggestion(creditCardNumber: String) {
        creditCardSuggestion(creditCardNumber).waitForExists(waitingTime)
        creditCardSuggestion(creditCardNumber).click()
    }

    fun verifyCreditCardSuggestion(vararg creditCardNumbers: String) {
        for (creditCardNumber in creditCardNumbers) {
            assertTrue(creditCardSuggestion(creditCardNumber).waitForExists(waitingTime))
        }
    }

    fun verifySuggestedUserName(userName: String) {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/mozac_feature_login_multiselect_expand"),
        ).waitForExists(waitingTime)

        assertTrue(
            mDevice.findObject(UiSelector().textContains(userName)).waitForExists(waitingTime),
        )
    }

    fun verifyPrefilledLoginCredentials(userName: String, password: String, credentialsArePrefilled: Boolean) {
        // Sometimes the assertion of the pre-filled logins fails so we are re-trying after refreshing the page
        for (i in 1..RETRY_COUNT) {
            try {
                if (credentialsArePrefilled) {
                    mDevice.waitForObjects(webPageItemWithResourceId("username"))
                    assertTrue(webPageItemWithResourceId("username").text.equals(userName))

                    mDevice.waitForObjects(webPageItemWithResourceId("password"))
                    assertTrue(webPageItemWithResourceId("password").text.equals(password))
                } else {
                    mDevice.waitForObjects(webPageItemWithResourceId("username"))
                    assertFalse(webPageItemWithResourceId("username").text.equals(userName))

                    mDevice.waitForObjects(webPageItemWithResourceId("password"))
                    assertFalse(webPageItemWithResourceId("password").text.equals(password))
                }

                break
            } catch (e: AssertionError) {
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    browserScreen {
                    }.openThreeDotMenu {
                    }.refreshPage {
                        clearUserNameLoginCredential()
                        clickSuggestedLoginsButton()
                        verifySuggestedUserName(userName)
                        clickLoginSuggestion(userName)
                        clickShowPasswordButton()
                    }
                }
            }
        }
    }

    fun verifyAutofilledAddress(streetAddress: String) {
        mDevice.waitForObjects(webPageItemContainingTextAndResourceId("streetAddress", streetAddress))
        assertTrue(
            webPageItemContainingTextAndResourceId("streetAddress", streetAddress)
                .waitForExists(waitingTime),
        )
    }

    fun verifyManuallyFilledAddress(apartment: String) {
        mDevice.waitForObjects(webPageItemContainingTextAndResourceId("apartment", apartment))
        assertTrue(
            webPageItemContainingTextAndResourceId("apartment", apartment)
                .waitForExists(waitingTime),
        )
    }

    fun verifyAutofilledCreditCard(creditCardNumber: String) {
        mDevice.waitForObjects(webPageItemContainingTextAndResourceId("cardNumber", creditCardNumber))
        assertTrue(
            webPageItemContainingTextAndResourceId("cardNumber", creditCardNumber)
                .waitForExists(waitingTime),
        )
    }

    fun verifyPrefilledPWALoginCredentials(userName: String, shortcutTitle: String) {
        mDevice.waitForIdle(waitingTime)

        var currentTries = 0
        while (currentTries++ < 3) {
            try {
                assertTrue(webPageItemWithResourceId("submit").waitForExists(waitingTime))
                webPageItemWithResourceId("submit").click()
                assertTrue(webPageItemWithResourceId("username").text.equals(userName))
                break
            } catch (e: AssertionError) {
                addToHomeScreen {
                }.searchAndOpenHomeScreenShortcut(shortcutTitle) {}
            }
        }
    }

    fun verifySaveLoginPromptIsDisplayed() {
        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/feature_prompt_login_fragment"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifySaveLoginPromptIsNotDisplayed() =
        assertItemWithResIdExists(
            itemWithResId("$packageName:id/feature_prompt_login_fragment"),
            exists = false,
        )

    fun verifyTrackingProtectionWebContent(state: String) {
        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(
                    mDevice.findObject(
                        UiSelector().textContains(state),
                    ).waitForExists(waitingTimeLong),
                )

                break
            } catch (e: AssertionError) {
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    Log.e("TestLog", "On try $i, trackers are not: $state")

                    navigationToolbar {
                    }.openThreeDotMenu {
                    }.refreshPage {
                    }
                }
            }
        }
    }

    fun clickSetCookiesButton() = clickPageObject(webPageItemWithResourceId("setCookies"))

    fun verifyCookiesProtectionHint() {
        val hintMessage =
            mDevice.findObject(
                UiSelector()
                    .textContains(getStringResource(R.string.tcp_cfr_message)),
            )
        assertTrue(hintMessage.waitForExists(waitingTime))
    }

    fun clickForm(formType: String) {
        when (formType) {
            "Calendar Form" -> {
                clickPageObject(webPageItemWithResourceId("calendar"))
                mDevice.waitForIdle(waitingTime)
            }
            "Clock Form" -> {
                clickPageObject(webPageItemWithResourceId("clock"))
                mDevice.waitForIdle(waitingTime)
            }
            "Color Form" -> {
                clickPageObject(webPageItemWithResourceId("colorPicker"))
                mDevice.waitForIdle(waitingTime)
            }
            "Drop-down Form" -> {
                clickPageObject(webPageItemWithResourceId("dropDown"))
                mDevice.waitForIdle(waitingTime)
            }
        }
    }

    fun clickFormViewButton(button: String) = mDevice.findObject(UiSelector().textContains(button)).click()

    fun selectDate() {
        mDevice.findObject(UiSelector().resourceId("android:id/month_view")).waitForExists(waitingTime)

        mDevice.findObject(
            UiSelector()
                .textContains("$currentDay")
                .descriptionContains("$currentDay $currentMonth $currentYear"),
        ).click()
    }

    fun selectTime(hour: Int, minute: Int) =
        onView(
            isAssignableFrom(TimePicker::class.java),
        ).inRoot(
            isDialog(),
        ).perform(PickerActions.setTime(hour, minute))

    fun selectColor(hexValue: String) {
        mDevice.findObject(
            UiSelector()
                .textContains("Choose a color")
                .resourceId("$packageName:id/alertTitle"),
        ).waitForExists(waitingTime)

        val colorSelection =
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/color_item")
                    .descriptionContains(hexValue),
            )
        colorSelection.click()
    }

    fun selectDropDownOption(optionName: String) {
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/customPanel"),
        ).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains(optionName)).click()
    }

    fun clickSubmitDateButton() = clickPageObject(webPageItemWithResourceId("submitDate"))

    fun clickSubmitTimeButton() = clickPageObject(webPageItemWithResourceId("submitTime"))

    fun clickSubmitColorButton() = clickPageObject(webPageItemWithResourceId("submitColor"))

    fun clickSubmitDropDownButton() = clickPageObject(webPageItemWithResourceId("submitOption"))

    fun verifySelectedDate() {
        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(
                    mDevice.findObject(
                        UiSelector()
                            .text("Selected date is: $currentDate"),
                    ).waitForExists(waitingTime),
                )

                break
            } catch (e: AssertionError) {
                Log.e("TestLog", "Selected time isn't displayed ${e.localizedMessage}")

                clickForm("Calendar Form")
                selectDate()
                clickFormViewButton("OK")
                clickSubmitDateButton()
            }
        }

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .text("Selected date is: $currentDate"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyNoDateIsSelected() {
        assertFalse(
            mDevice.findObject(
                UiSelector()
                    .text("Selected date is: $currentDate"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifySelectedTime(hour: Int, minute: Int) {
        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(
                    mDevice.findObject(
                        UiSelector()
                            .text("Selected time is: $hour:$minute"),
                    ).waitForExists(waitingTime),
                )

                break
            } catch (e: AssertionError) {
                Log.e("TestLog", "Selected time isn't displayed ${e.localizedMessage}")

                clickForm("Clock Form")
                clickFormViewButton("CLEAR")
                clickForm("Clock Form")
                selectTime(hour, minute)
                clickFormViewButton("OK")
                clickSubmitTimeButton()
            }
        }

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .text("Selected time is: $hour:$minute"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifySelectedColor(hexValue: String) {
        for (i in 1..RETRY_COUNT) {
            try {
                assertTrue(
                    mDevice.findObject(
                        UiSelector()
                            .text("Selected color is: $hexValue"),
                    ).waitForExists(waitingTime),
                )

                break
            } catch (e: AssertionError) {
                Log.e("TestLog", "Selected color isn't displayed ${e.localizedMessage}")

                clickForm("Color Form")
                selectColor(hexValue)
                clickFormViewButton("SET")
                clickSubmitColorButton()
            }
        }

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .text("Selected color is: $hexValue"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifySelectedDropDownOption(optionName: String) {
        for (i in 1..RETRY_COUNT) {
            try {
                mDevice.findObject(
                    UiSelector()
                        .textContains("Submit drop down option")
                        .resourceId("submitOption"),
                ).waitForExists(waitingTime)

                assertTrue(
                    mDevice.findObject(
                        UiSelector()
                            .text("Selected option is: $optionName"),
                    ).waitForExists(waitingTime),
                )

                break
            } catch (e: AssertionError) {
                Log.e("TestLog", "Selected option isn't displayed ${e.localizedMessage}")

                clickForm("Drop-down Form")
                selectDropDownOption(optionName)
                clickSubmitDropDownButton()
            }
        }

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .text("Selected option is: $optionName"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyNoTimeIsSelected(hour: Int, minute: Int) {
        assertFalse(
            mDevice.findObject(
                UiSelector()
                    .text("Selected date is: $hour:$minute"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyColorIsNotSelected(hexValue: String) {
        assertFalse(
            mDevice.findObject(
                UiSelector()
                    .text("Selected date is: $hexValue"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyCookieBannerExists(exists: Boolean) {
        for (i in 1..RETRY_COUNT) {
            try {
                assertItemWithResIdExists(cookieBanner, exists = exists)

                break
            } catch (e: AssertionError) {
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    browserScreen {
                    }.openThreeDotMenu {
                    }.refreshPage {
                        waitForPageToLoad()
                    }
                }
            }
        }
        assertItemWithResIdExists(cookieBanner, exists = exists)
    }

    fun clickShowPasswordButton() =
        itemWithResId("togglePassword").also {
            it.waitForExists(waitingTime)
            it.click()
        }

    fun verifyOpenLinkInAnotherAppPrompt() {
        assertItemWithResIdExists(itemWithResId("$packageName:id/parentPanel"))
        assertItemContainingTextExists(
            itemContainingText(
                getStringResource(R.string.mozac_feature_applinks_normal_confirm_dialog_title),
            ),
            itemContainingText(
                getStringResource(R.string.mozac_feature_applinks_normal_confirm_dialog_message),
            ),
        )
    }

    fun verifyPrivateBrowsingOpenLinkInAnotherAppPrompt(url: String) =
        assertItemContainingTextExists(
            itemContainingText(
                getStringResource(R.string.mozac_feature_applinks_confirm_dialog_title),
            ),
            itemContainingText(url),
        )

    fun confirmOpenLinkInAnotherApp() =
        itemWithResIdAndText("android:id/button1", "OPEN")
            .also {
                it.waitForExists(waitingTime)
                it.click()
            }

    fun cancelOpenLinkInAnotherApp() =
        itemWithResIdAndText("android:id/button2", "CANCEL")
            .also {
                it.waitForExists(waitingTime)
                it.click()
            }

    fun verifyFindInPageBar(exists: Boolean) =
        assertItemWithResIdExists(
            itemWithResId("$packageName:id/findInPageView"),
            exists = exists,
        )

    class Transition {
        private fun threeDotButton() = onView(
            allOf(
                ViewMatchers.withContentDescription(
                    "Menu",
                ),
            ),
        )

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            threeDotButton().perform(ViewActions.click())

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            navURLBar().waitForExists(waitingTime)
            navURLBar().click()
            mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_url_view"))
                .waitForExists(waitingTime)

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openTabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            for (i in 1..RETRY_COUNT) {
                try {
                    mDevice.waitForObjects(
                        mDevice.findObject(
                            UiSelector()
                                .resourceId("$packageName:id/mozac_browser_toolbar_browser_actions"),
                        ),
                        waitingTime,
                    )

                    tabsCounter().click()
                    assertTrue(
                        MatcherHelper.itemWithResId("$packageName:id/new_tab_button")
                            .waitForExists(waitingTime),
                    )

                    break
                } catch (e: AssertionError) {
                    if (i == RETRY_COUNT) {
                        throw e
                    } else {
                        mDevice.waitForIdle()
                    }
                }
            }
            assertTrue(MatcherHelper.itemWithResId("$packageName:id/new_tab_button").waitForExists(waitingTime))

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun openTabButtonShortcutsMenu(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            mDevice.waitNotNull(Until.findObject(By.desc("Tabs")))
            tabsCounter().click(LONG_CLICK_DURATION)

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openNotificationShade(interact: NotificationRobot.() -> Unit): NotificationRobot.Transition {
            mDevice.openNotification()

            NotificationRobot().interact()
            return NotificationRobot.Transition()
        }

        fun goToHomescreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            assertTrue(
                mDevice.findObject(UiSelector().description("Home screen"))
                    .waitForExists(waitingTime),
            )

            onView(withContentDescription("Home screen"))
                .check(matches(isDisplayed()))
                .click()

            assertTrue(
                mDevice.findObject(UiSelector().resourceId("$packageName:id/homeLayout"))
                    .waitForExists(waitingTime) ||
                    mDevice.findObject(
                        UiSelector().text(
                            getStringResource(R.string.onboarding_home_screen_jump_back_contextual_hint_2),
                        ),
                    ).waitForExists(waitingTime),
            )

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.pressBack()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun clickTabCrashedCloseButton(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            assertTrue(
                mDevice.findObject(UiSelector().resourceId("$packageName:id/closeTabButton"))
                    .waitForExists(waitingTime),
            )

            val tabCrashedCloseButton = mDevice.findObject(text("Close tab"))
            tabCrashedCloseButton.click()

            mDevice.waitForIdle()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun clickShareSelectedText(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            val shareTextButton = mDevice.findObject(By.textContains("Share"))
            shareTextButton.click()

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun clickDownloadLink(title: String, interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
            assertTrue(
                "$title download link not found",
                webPageItemContainingText(title).waitForExists(waitingTime),
            )

            clickPageObject(webPageItemContainingText(title))

            DownloadRobot().interact()
            return DownloadRobot.Transition()
        }

        fun clickStartCameraButton(interact: SitePermissionsRobot.() -> Unit): SitePermissionsRobot.Transition {
            // Test page used for testing permissions located at https://mozilla-mobile.github.io/testapp/permissions
            clickPageObject(webPageItemWithText("Open camera"))

            SitePermissionsRobot().interact()
            return SitePermissionsRobot.Transition()
        }

        fun clickStartMicrophoneButton(interact: SitePermissionsRobot.() -> Unit): SitePermissionsRobot.Transition {
            // Test page used for testing permissions located at https://mozilla-mobile.github.io/testapp/permissions
            clickPageObject(webPageItemWithText("Open microphone"))

            SitePermissionsRobot().interact()
            return SitePermissionsRobot.Transition()
        }

        fun clickStartAudioVideoButton(interact: SitePermissionsRobot.() -> Unit): SitePermissionsRobot.Transition {
            // Test page used for testing permissions located at https://mozilla-mobile.github.io/testapp/permissions
            clickPageObject(webPageItemWithText("Camera & Microphone"))

            SitePermissionsRobot().interact()
            return SitePermissionsRobot.Transition()
        }

        fun clickOpenNotificationButton(interact: SitePermissionsRobot.() -> Unit): SitePermissionsRobot.Transition {
            // Test page used for testing permissions located at https://mozilla-mobile.github.io/testapp/permissions
            clickPageObject(webPageItemWithText("Open notifications dialogue"))
            mDevice.waitForObjects(mDevice.findObject(UiSelector().textContains("Allow to send notifications?")))

            SitePermissionsRobot().interact()
            return SitePermissionsRobot.Transition()
        }

        fun clickGetLocationButton(interact: SitePermissionsRobot.() -> Unit): SitePermissionsRobot.Transition {
            // Test page used for testing permissions located at https://mozilla-mobile.github.io/testapp/permissions
            clickPageObject(webPageItemWithText("Get Location"))

            SitePermissionsRobot().interact()
            return SitePermissionsRobot.Transition()
        }

        fun clickRequestStorageAccessButton(interact: SitePermissionsRobot.() -> Unit): SitePermissionsRobot.Transition {
            clickPageObject(webPageItemContainingText("requestStorageAccess()"))

            SitePermissionsRobot().interact()
            return SitePermissionsRobot.Transition()
        }

        fun openSiteSecuritySheet(interact: SiteSecurityRobot.() -> Unit): SiteSecurityRobot.Transition {
            siteSecurityToolbarButton().waitForExists(waitingTime)
            siteSecurityToolbarButton().clickAndWaitForNewWindow(waitingTime)

            SiteSecurityRobot().interact()
            return SiteSecurityRobot.Transition()
        }

        fun clickManageAddressButton(interact: SettingsSubMenuAutofillRobot.() -> Unit): SettingsSubMenuAutofillRobot.Transition {
            itemWithResId("$packageName:id/manage_addresses").clickAndWaitForNewWindow(waitingTime)

            SettingsSubMenuAutofillRobot().interact()
            return SettingsSubMenuAutofillRobot.Transition()
        }

        fun clickManageCreditCardsButton(interact: SettingsSubMenuAutofillRobot.() -> Unit): SettingsSubMenuAutofillRobot.Transition {
            itemWithResId("$packageName:id/manage_credit_cards").clickAndWaitForNewWindow(waitingTime)

            SettingsSubMenuAutofillRobot().interact()
            return SettingsSubMenuAutofillRobot.Transition()
        }
    }
}

fun browserScreen(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
    BrowserRobot().interact()
    return BrowserRobot.Transition()
}

fun navURLBar() = mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))

fun searchBar() = onView(withId(R.id.mozac_browser_toolbar_url_view))

fun homeScreenButton() = onView(withContentDescription(R.string.browser_toolbar_home))

private fun assertHomeScreenButton() =
    homeScreenButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSearchBar() = searchBar().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNavURLBar() = assertTrue(navURLBar().waitForExists(waitingTime))

private fun assertNavURLBarHidden() = assertTrue(navURLBar().waitUntilGone(waitingTime))

private fun assertSecureConnectionLockIcon() {
    onView(withId(R.id.mozac_browser_toolbar_security_indicator))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun menuButton() = onView(withId(R.id.icon))

private fun assertMenuButton() {
    menuButton()
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun tabsCounter() =
    mDevice.findObject(By.res("$packageName:id/mozac_browser_toolbar_browser_actions"))

private var progressBar =
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_progress"),
    )

private val suggestedLogins = mDevice.findObject(UiSelector().resourceId("$packageName:id/loginSelectBar"))
private val selectAddressButton = mDevice.findObject(UiSelector().resourceId("$packageName:id/select_address_header"))
private val selectCreditCardButton = mDevice.findObject(UiSelector().resourceId("$packageName:id/select_credit_card_header"))

private fun addressSuggestion(streetName: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/address_name")
            .textContains(streetName),
    )

private fun creditCardSuggestion(creditCardNumber: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/credit_card_number")
            .textContains(creditCardNumber),
    )

private fun siteSecurityToolbarButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_security_indicator"))

private fun clickPageObject(webPageItem: UiObject) {
    for (i in 1..RETRY_COUNT) {
        try {
            webPageItem.also {
                it.waitForExists(waitingTime)
                it.click()
            }

            break
        } catch (e: UiObjectNotFoundException) {
            if (i == RETRY_COUNT) {
                throw e
            } else {
                browserScreen {
                }.openThreeDotMenu {
                }.refreshPage {
                    progressBar.waitUntilGone(waitingTime)
                }
            }
        }
    }
}

fun longClickPageObject(webPageItem: UiObject) {
    for (i in 1..RETRY_COUNT) {
        try {
            webPageItem.also {
                it.waitForExists(waitingTime)
                it.longClick()
            }

            break
        } catch (e: UiObjectNotFoundException) {
            if (i == RETRY_COUNT) {
                throw e
            } else {
                browserScreen {
                }.openThreeDotMenu {
                }.refreshPage {
                }
                progressBar.waitUntilGone(waitingTime)
            }
        }
    }
}

private fun webPageItemContainingText(itemText: String) =
    mDevice.findObject(UiSelector().textContains(itemText))

private fun webPageItemWithText(itemText: String) =
    mDevice.findObject(UiSelector().text(itemText))

private fun webPageItemWithResourceId(resourceId: String) =
    mDevice.findObject(UiSelector().resourceId(resourceId))

private fun webPageItemContainingTextAndResourceId(resourceId: String, itemText: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId(resourceId)
            .textContains(itemText),
    )

private fun setPageObjectText(webPageItem: UiObject, text: String) {
    for (i in 1..RETRY_COUNT) {
        try {
            webPageItem.also {
                it.waitForExists(waitingTime)
                it.clearTextField()
                it.text = text
            }

            break
        } catch (e: UiObjectNotFoundException) {
            if (i == RETRY_COUNT) {
                throw e
            } else {
                browserScreen {
                }.openThreeDotMenu {
                }.refreshPage {
                    progressBar.waitUntilGone(waitingTime)
                }
            }
        }
    }
}

private val currentDate = LocalDate.now()
private val currentDay = currentDate.dayOfMonth
private val currentMonth = currentDate.month
private val currentYear = currentDate.year
private val cookieBanner = itemWithResId("CybotCookiebotDialog")
