/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.focus.activity

import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.activity.robots.browserScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.FeatureSettingsHelper
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.createMockResponseFromAsset
import org.mozilla.focus.testAnnotations.SmokeTest
import java.io.IOException

class ShortcutsTest {
    private lateinit var webServer: MockWebServer
    private val featureSettingsHelper = FeatureSettingsHelper()

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @Before
    fun setUp() {
        featureSettingsHelper.setShieldIconCFREnabled(false)
        webServer = MockWebServer()
        webServer.enqueue(createMockResponseFromAsset("plain_test.html"))
        webServer.start()
    }

    @After
    fun tearDown() {
        try {
            webServer.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
        featureSettingsHelper.resetAllFeatureFlags()
    }

    @SmokeTest
    @Test
    fun renameShortcutTest() {
        val webPage = object {
            val url = webServer.url("plain_test.html").toString()
            val title = "Plain text page"
            val newTitle = "TestShortcut"
        }

        searchScreen {
        }.loadPage(webPage.url) {
            verifyPageContent(webPage.title)
        }.openMainMenu {
            clickAddToShortcuts()
        }
        browserScreen {
        }.clearBrowsingData {
            verifyPageShortcutExists(webPage.title)
            longTapPageShortcut(webPage.title)
            clickRenameShortcut()
            renameShortcutAndSave(webPage.newTitle)
            verifyPageShortcutExists(webPage.newTitle)
        }
    }

    @SmokeTest
    @Test
    fun searchBarShowsPageShortcutsTest() {
        val webPage = object {
            val url = webServer.url("plain_test.html").toString()
            val title = "Plain text page"
        }

        searchScreen {
        }.loadPage(webPage.url) {
            verifyPageContent(webPage.title)
        }.openMainMenu {
            clickAddToShortcuts()
        }
        browserScreen {
        }.clearBrowsingData {
            verifyPageShortcutExists(webPage.title)
        }.clickPageShortcut(webPage.title) {
        }.openSearchBar {
            verifySearchSuggestionsContain(webPage.title)
        }
    }
}
