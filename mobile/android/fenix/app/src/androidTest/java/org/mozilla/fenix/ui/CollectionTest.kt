/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.collectionRobot
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.tabDrawer

/**
 *  Tests for verifying basic functionality of tab collections
 *
 */

class CollectionTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer
    private val firstCollectionName = "testcollection_1"
    private val secondCollectionName = "testcollection_2"
    private val collectionName = "First Collection"

    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                isHomeOnboardingDialogEnabled = false,
                isJumpBackInCFREnabled = false,
                isRecentTabsFeatureEnabled = false,
                isRecentlyVisitedFeatureEnabled = false,
                isPocketEnabled = false,
                isWallpaperOnboardingEnabled = false,
                isTCPCFREnabled = false,
            ),
        ) { it.activity }

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @SmokeTest
    @Test
    fun createFirstCollectionTest() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            mDevice.waitForIdle()
        }.goToHomescreen {
        }.clickSaveTabsToCollectionButton {
            longClickTab(firstWebPage.title)
            selectTab(secondWebPage.title, numOfTabs = 2)
        }.clickSaveCollection {
            typeCollectionNameAndSave(collectionName)
        }

        tabDrawer {
            verifySnackBarText("Collection saved!")
            snackBarButtonClick("VIEW")
        }

        homeScreen {
            verifyCollectionIsDisplayed(collectionName)
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812580")
    @SmokeTest
    @Test
    fun verifyExpandedCollectionItemsTest() {
        val webPage = getGenericAsset(mockWebServer, 1)
        val webPageUrl = webPage.url.host.toString()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName = collectionName)
            snackBarButtonClick("VIEW")
        }

        homeScreen {
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName, composeTestRule) {
            verifyTabSavedInCollection(webPage.title)
            verifyCollectionTabUrl(true, webPageUrl)
            verifyShareCollectionButtonIsVisible(true)
            verifyCollectionMenuIsVisible(true, composeTestRule)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, true)
        }.collapseCollection(collectionName) {}

        collectionRobot {
            verifyTabSavedInCollection(webPage.title, false)
            verifyShareCollectionButtonIsVisible(false)
            verifyCollectionMenuIsVisible(false, composeTestRule)
            verifyCollectionTabUrl(false, webPageUrl)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, false)
        }

        homeScreen {
        }.expandCollection(collectionName, composeTestRule) {
            verifyTabSavedInCollection(webPage.title)
            verifyCollectionTabUrl(true, webPageUrl)
            verifyShareCollectionButtonIsVisible(true)
            verifyCollectionMenuIsVisible(true, composeTestRule)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, true)
        }.collapseCollection(collectionName) {}

        collectionRobot {
            verifyTabSavedInCollection(webPage.title, false)
            verifyShareCollectionButtonIsVisible(false)
            verifyCollectionMenuIsVisible(false, composeTestRule)
            verifyCollectionTabUrl(false, webPageUrl)
            verifyCollectionItemRemoveButtonIsVisible(webPage.title, false)
        }
    }

    @SmokeTest
    @Test
    fun openAllTabsInCollectionTest() {
        val firstTestPage = getGenericAsset(mockWebServer, 1)
        val secondTestPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstTestPage.url) {
            waitForPageToLoad()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondTestPage.url.toString()) {
            waitForPageToLoad()
        }.openTabDrawer {
            createCollection(
                firstTestPage.title,
                secondTestPage.title,
                collectionName = collectionName,
            )
            closeTab()
        }

        homeScreen {
        }.expandCollection(collectionName, composeTestRule) {
            clickCollectionThreeDotButton(composeTestRule)
            selectOpenTabs(composeTestRule)
        }
        tabDrawer {
            verifyExistingOpenTabs(firstTestPage.title, secondTestPage.title)
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812580")
    @SmokeTest
    @Test
    fun shareCollectionTest() {
        val firstWebsite = getGenericAsset(mockWebServer, 1)
        val secondWebsite = getGenericAsset(mockWebServer, 2)
        val sharingApp = "Gmail"
        val urlString = "${secondWebsite.url}\n\n${firstWebsite.url}"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebsite.url) {
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebsite.url.toString()) {
            waitForPageToLoad()
        }.openTabDrawer {
            createCollection(firstWebsite.title, secondWebsite.title, collectionName = collectionName)
            verifySnackBarText("Collection saved!")
        }.openTabsListThreeDotMenu {
        }.closeAllTabs {
        }.expandCollection(collectionName, composeTestRule) {
        }.clickShareCollectionButton {
            verifyShareTabsOverlay(firstWebsite.title, secondWebsite.title)
            verifySharingWithSelectedApp(sharingApp, urlString, collectionName)
        }
    }

    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812580")
    @SmokeTest
    @Test
    fun deleteCollectionTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName = collectionName)
            snackBarButtonClick("VIEW")
        }

        homeScreen {
        }.expandCollection(collectionName, composeTestRule) {
            clickCollectionThreeDotButton(composeTestRule)
            selectDeleteCollection(composeTestRule)
        }

        homeScreen {
            verifySnackBarText("Collection deleted")
            verifyNoCollectionsText()
        }
    }

    // open a webpage, and add currently opened tab to existing collection
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812580")
    @Test
    fun mainMenuSaveToExistingCollection() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
            createCollection(firstWebPage.title, collectionName = collectionName)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {}

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            verifyPageContent(secondWebPage.content)
        }.openThreeDotMenu {
        }.openSaveToCollection {
        }.selectExistingCollection(collectionName) {
            verifySnackBarText("Tab saved!")
        }.goToHomescreen {
        }.expandCollection(collectionName, composeTestRule) {
            verifyTabSavedInCollection(firstWebPage.title)
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812580")
    @Test
    fun verifyAddTabButtonOfCollectionMenu() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
            createCollection(firstWebPage.title, collectionName = collectionName)
            verifySnackBarText("Collection saved!")
            closeTab()
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
        }.goToHomescreen {
        }.expandCollection(collectionName, composeTestRule) {
            clickCollectionThreeDotButton(composeTestRule)
            selectAddTabToCollection(composeTestRule)
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
            createCollection(webPage.title, collectionName = firstCollectionName)
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {
        }.goToHomescreen {
        }.expandCollection(firstCollectionName, composeTestRule) {
            clickCollectionThreeDotButton(composeTestRule)
            selectRenameCollection(composeTestRule)
        }.typeCollectionNameAndSave(secondCollectionName) {}
        homeScreen {
            verifyCollectionIsDisplayed(secondCollectionName)
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812580")
    @Test
    fun createSecondCollectionTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName = firstCollectionName)
            verifySnackBarText("Collection saved!")
            createCollection(
                webPage.title,
                collectionName = secondCollectionName,
                firstCollection = false,
            )
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {
        }.goToHomescreen {
            verifyCollectionIsDisplayed(firstCollectionName)
            verifyCollectionIsDisplayed(secondCollectionName)
        }
    }

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1812580")
    @Test
    fun removeTabFromCollectionTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName = collectionName)
            closeTab()
        }

        homeScreen {
        }.expandCollection(collectionName, composeTestRule) {
            verifyTabSavedInCollection(webPage.title, true)
            removeTabFromCollection(webPage.title)
            verifyTabSavedInCollection(webPage.title, false)
        }
        homeScreen {
            verifyCollectionIsDisplayed(collectionName, false)
        }
    }

    @Ignore("Failing, see: Bugzilla tickets 1807289 and 1812997")
    @Test
    fun swipeLeftToRemoveTabFromCollectionTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            waitForPageToLoad()
        }.openTabDrawer {
            createCollection(
                testPage.title,
                collectionName = collectionName,
            )
            closeTab()
        }

        homeScreen {
        }.expandCollection(collectionName, composeTestRule) {
            swipeToBottom()
            swipeTabLeft(testPage.title, composeTestRule)
            verifyTabSavedInCollection(testPage.title, false)
        }
        homeScreen {
            verifyCollectionIsDisplayed(collectionName, false)
        }
    }

    @Test
    fun swipeRightToRemoveTabFromCollectionTest() {
        val testPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(testPage.url) {
            waitForPageToLoad()
        }.openTabDrawer {
            createCollection(
                testPage.title,
                collectionName = collectionName,
            )
            closeTab()
        }

        homeScreen {
        }.expandCollection(collectionName, composeTestRule) {
            swipeToBottom()
            swipeTabRight(testPage.title, composeTestRule)
            verifyTabSavedInCollection(testPage.title, false)
        }
        homeScreen {
            verifyCollectionIsDisplayed(collectionName, false)
        }
    }

    @Test
    @Ignore("Failing after compose migration. See: https://github.com/mozilla-mobile/fenix/issues/26087")
    fun selectTabOnLongTapTest() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            waitForPageToLoad()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            waitForPageToLoad()
        }.openTabDrawer {
            verifyExistingOpenTabs(firstWebPage.title, secondWebPage.title)
            longClickTab(firstWebPage.title)
            verifyTabsMultiSelectionCounter(1)
            selectTab(secondWebPage.title, numOfTabs = 2)
        }.clickSaveCollection {
            typeCollectionNameAndSave(collectionName)
            verifySnackBarText("Tabs saved!")
        }

        tabDrawer {
        }.closeTabDrawer {
        }.goToHomescreen {
        }.expandCollection(collectionName, composeTestRule) {
            verifyTabSavedInCollection(firstWebPage.title)
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    @Test
    @Ignore("Failing after compose migration. See: https://github.com/mozilla-mobile/fenix/issues/26087")
    fun navigateBackInCollectionFlowTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName = collectionName)
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

    @SmokeTest
    @Test
    @Ignore("Failing after compose migration. See: https://github.com/mozilla-mobile/fenix/issues/26087")
    fun undoDeleteCollectionTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName = collectionName)
            snackBarButtonClick("VIEW")
        }

        homeScreen {
        }.expandCollection(collectionName, composeTestRule) {
            clickCollectionThreeDotButton(composeTestRule)
            selectDeleteCollection(composeTestRule)
        }

        homeScreen {
            verifySnackBarText("Collection deleted")
            clickUndoSnackBarButton()
            verifyCollectionIsDisplayed(collectionName, true)
        }
    }
}
