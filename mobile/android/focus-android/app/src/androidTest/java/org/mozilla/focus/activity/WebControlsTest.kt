/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.intent.Intents
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.Constants.PackageName.GMAIL_APP
import org.mozilla.focus.helpers.Constants.PackageName.PHONE_APP
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.RetryTestRule
import org.mozilla.focus.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.testAnnotations.SmokeTest

// These tests check that various web controls work properly
@RunWith(AndroidJUnit4ClassRunner::class)
class WebControlsTest {
    private lateinit var webServer: MockWebServer

    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setup() {
        webServer = MockWebServer()
        webServer.start()
        featureSettingsHelper.setCfrForTrackingProtectionEnabled(false)
        featureSettingsHelper.setNumberOfTabsOpened(4)
        Intents.init()
    }

    @After
    fun tearDown() {
        webServer.shutdown()
        featureSettingsHelper.resetAllFeatureFlags()
        Intents.release()
    }

    @SmokeTest
    @Test
    fun verifyTextInputTest() {
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickAndWriteTextInInputBox("wiki")
            clickSubmitTextInputButton()
            verifyPageContent("You entered: wiki")
        }
    }

    @SmokeTest
    @Test
    fun verifyDropdownMenuTest() {
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickDropDownForm()
            selectDropDownOption("The National")
            clickSubmitDropDownButton()
            verifySelectedDropDownOption("The National")
        }
    }

    @SmokeTest
    @Test
    fun verifyExternalLinksTest() {
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickLinkMatchingText("External link")
            progressBar.waitUntilGone(waitingTime)
            verifyPageURL("DuckDuckGo")
        }
    }

    @SmokeTest
    @Test
    fun emailLinkTest() {
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(htmlControlsPage) {
            clickLinkMatchingText("Email link")
            clickOpenLinksInAppsOpenButton()
            assertNativeAppOpens(GMAIL_APP)
        }
    }

    @SmokeTest
    @Test
    fun telephoneLinkTest() {
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(htmlControlsPage) {
            clickLinkMatchingText("Telephone link")
            clickOpenLinksInAppsOpenButton()
            assertNativeAppOpens(PHONE_APP)
        }
    }

    @SmokeTest
    @Test
    fun verifyDismissTextSelectionToolbarTest() {
        webServer.enqueue(createMockResponseFromAsset("tab1.html"))
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))

        val tab1Url = webServer.url("tab1.html").toString()
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(tab1Url) {
            progressBar.waitUntilGone(waitingTime)
        }.openSearchBar {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            longClickText("Copy")
        }.goToPreviousPage {
            verifyCopyOptionDoesNotExist()
        }
    }

    @SmokeTest
    @Test
    fun verifySelectTextTest() {
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            longClickAndCopyText("Copy")
            clickAndPasteTextInInputBox()
            clickSubmitTextInputButton()
            verifyPageContent("You entered: Copy")
        }
    }

    @SmokeTest
    @Test
    fun verifyCalendarFormTest() {
        webServer.enqueue(createMockResponseFromAsset("htmlControls.html"))
        val htmlControlsPage = webServer.url("htmlControls.html").toString()

        searchScreen {
        }.loadPage(htmlControlsPage) {
            progressBar.waitUntilGone(waitingTime)
            clickCalendarForm()
            selectDate()
            clickFormViewButton("OK")
            clickSubmitDateButton()
            verifySelectedDate()
        }
    }
}
