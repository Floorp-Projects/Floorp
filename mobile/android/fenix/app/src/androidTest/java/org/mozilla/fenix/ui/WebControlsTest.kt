/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.Constants
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.assertNativeAppOpens
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic interactions with web control elements
 *
 */

class WebControlsTest {
    private lateinit var mockWebServer: MockWebServer

    private val hour = 10
    private val minute = 10
    private val colorHexValue = "#5b2067"
    private val emailLink = "mailto://example@example.com"
    private val phoneLink = "tel://1234567890"

    @get:Rule
    val activityTestRule = HomeActivityTestRule(
        isJumpBackInCFREnabled = false,
        isTCPCFREnabled = false,
    )

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun cancelCalendarFormTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(htmlControlsPage.url) {
            clickForm("Calendar Form")
            clickFormViewButton("CANCEL")
            clickSubmitDateButton()
            verifyNoDateIsSelected()
        }
    }

    @Test
    fun setAndClearCalendarFormTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(htmlControlsPage.url) {
            clickForm("Calendar Form")
            selectDate()
            clickFormViewButton("OK")
            clickSubmitDateButton()
            verifySelectedDate()
            clickForm("Calendar Form")
            clickFormViewButton("CLEAR")
            clickSubmitDateButton()
            verifyNoDateIsSelected()
        }
    }

    @Test
    fun cancelClockFormTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(htmlControlsPage.url) {
            clickForm("Clock Form")
            clickFormViewButton("CANCEL")
            clickSubmitTimeButton()
            verifyNoTimeIsSelected(hour, minute)
        }
    }

    @Test
    fun setAndClearClockFormTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(htmlControlsPage.url) {
            clickForm("Clock Form")
            selectTime(hour, minute)
            clickFormViewButton("OK")
            clickSubmitTimeButton()
            verifySelectedTime(hour, minute)
            clickForm("Clock Form")
            clickFormViewButton("CLEAR")
            clickSubmitTimeButton()
            verifyNoTimeIsSelected(hour, minute)
        }
    }

    @Test
    fun cancelColorFormTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(htmlControlsPage.url) {
            clickForm("Color Form")
            selectColor(colorHexValue)
            clickFormViewButton("CANCEL")
            clickSubmitColorButton()
            verifyColorIsNotSelected(colorHexValue)
        }
    }

    @Test
    fun setColorFormTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(htmlControlsPage.url) {
            clickForm("Color Form")
            selectColor(colorHexValue)
            clickFormViewButton("SET")
            clickSubmitColorButton()
            verifySelectedColor(colorHexValue)
        }
    }

    @Test
    fun verifyDropdownMenuTest() {
        val htmlControlsPage = TestAssetHelper.getHTMLControlsFormAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(htmlControlsPage.url) {
            clickForm("Drop-down Form")
            selectDropDownOption("The National")
            clickSubmitDropDownButton()
            verifySelectedDropDownOption("The National")
        }
    }

    @Test
    fun externalLinkTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickLinkMatchingText("External link")
            verifyUrl("duckduckgo")
        }
    }

    @Test
    fun emailLinkTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickLinkMatchingText("Email link")
            confirmOpenLinkInAnotherApp()
            assertNativeAppOpens(Constants.PackageName.GMAIL_APP, emailLink)
        }
    }

    @Test
    fun telephoneLinkTest() {
        val externalLinksPage = TestAssetHelper.getExternalLinksAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(externalLinksPage.url) {
            clickLinkMatchingText("Telephone link")
            confirmOpenLinkInAnotherApp()
            assertNativeAppOpens(Constants.PackageName.PHONE_APP, phoneLink)
        }
    }
}
