/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.tabDrawer

/**
 *  Tests for verifying basic functionality of tab collections
 *
 */

class CollectionTest {
    /* ktlint-disable no-blank-line-before-rbrace */
    // This imposes unreadable grouping.

    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer
    private val firstCollectionName = "testcollection_1"
    private val secondCollectionName = "testcollection_2"

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

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
    // open a webpage, and add currently opened tab to existing collection
    fun mainMenuSaveToExistingCollection() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
            createCollection(firstWebPage.title, firstCollectionName)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {}

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            verifyPageContent(secondWebPage.content)
        }.openThreeDotMenu {
        }.openSaveToCollection {
        }.selectExistingCollection(firstCollectionName) {
            verifySnackBarText("Tab saved!")
        }.goToHomescreen {
        }.expandCollection(firstCollectionName) {
            verifyTabSavedInCollection(firstWebPage.title)
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    @Test
    fun verifyAddTabButtonOfCollectionMenu() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
            createCollection(firstWebPage.title, firstCollectionName)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {}

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
        }.goToHomescreen {
        }.expandCollection(firstCollectionName) {
            clickCollectionThreeDotButton()
            selectAddTabToCollection()
            verifyTabsSelectedCounterText(1)
            saveTabsSelectedForCollection()
            verifySnackBarText("Tab saved!")
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    @Test
    fun renameCollectionTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, firstCollectionName)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {
        }.goToHomescreen {
        }.expandCollection(firstCollectionName) {
            clickCollectionThreeDotButton()
            selectRenameCollection()
        }.typeCollectionNameAndSave("renamed_collection") {}

        homeScreen {
            verifyCollectionIsDisplayed("renamed_collection")
        }
    }

    @Test
    fun createSecondCollectionTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, firstCollectionName)
            verifySnackBarText("Collection saved!")
            createCollection(webPage.title, secondCollectionName, false)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {
        }.goToHomescreen {}

        homeScreen {
            verifyCollectionIsDisplayed(firstCollectionName)
            verifyCollectionIsDisplayed(secondCollectionName)
        }
    }

    @Test
    fun removeTabFromCollectionTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, firstCollectionName)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {
        }.goToHomescreen {
        }.expandCollection(firstCollectionName) {
            removeTabFromCollection(webPage.title)
            verifyTabSavedInCollection(webPage.title, false)
        }
        // To add this step when https://github.com/mozilla-mobile/fenix/issues/13177 is fixed
//        homeScreen {
//            verifyCollectionIsDisplayed(firstCollectionName, false)
//        }
    }

    @Test
    fun swipeToRemoveTabFromCollectionTest() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
            createCollection(firstWebPage.title, firstCollectionName)
            verifySnackBarText("Collection saved!")
            closeTab()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
        }.openThreeDotMenu {
        }.openSaveToCollection {
        }.selectExistingCollection(firstCollectionName) {
        }.goToHomescreen {}

        homeScreen {
        }.expandCollection(firstCollectionName) {
            swipeCollectionItemLeft(firstWebPage.title)
            verifyTabSavedInCollection(firstWebPage.title, false)
            swipeCollectionItemRight(secondWebPage.title)
            verifyTabSavedInCollection(secondWebPage.title, false)
        }
        // To add this step when https://github.com/mozilla-mobile/fenix/issues/13177 is fixed
//        homeScreen {
//            verifyCollectionIsDisplayed(firstCollectionName, false)
//        }
    }

    @Test
    fun selectTabOnLongTapTest() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
        }.openTabDrawer {
            longClickTab(firstWebPage.title)
            verifyTabsMultiSelectionCounter(1)
            selectTab(secondWebPage.title)
            verifyTabsMultiSelectionCounter(2)
        }.clickSaveCollection {
            typeCollectionNameAndSave(firstCollectionName)
            verifySnackBarText("Tabs saved!")
        }

        tabDrawer {
        }.closeTabDrawer {
        }.goToHomescreen {
        }.expandCollection(firstCollectionName) {
            verifyTabSavedInCollection(firstWebPage.title)
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    @Test
    fun navigateBackInCollectionFlowTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, firstCollectionName)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {
        }.openThreeDotMenu {
        }.openSaveToCollection {
            verifySelectCollectionScreen()
            goBackInCollectionFlow()
        }

        browserScreen {
        }.openThreeDotMenu {
        }.openSaveToCollection {
            verifySelectCollectionScreen()
            clickAddNewCollection()
            verifyCollectionNameTextField()
            goBackInCollectionFlow()
            verifySelectCollectionScreen()
            goBackInCollectionFlow()
        }
        // verify the browser layout is visible
        browserScreen {
            verifyMenuButton()
        }
    }
}
