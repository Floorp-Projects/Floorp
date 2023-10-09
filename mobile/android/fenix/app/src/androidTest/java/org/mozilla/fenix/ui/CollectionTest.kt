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
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/353823
    @SmokeTest
    @Test
    fun createFirstCollectionUsingHomeScreenButtonTest() {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2283299
    @Test
    fun createFirstCollectionFromMainMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openSaveToCollection {
            verifyCollectionNameTextField()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/343422
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
        }.expandCollection(collectionName) {
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
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

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/343425
    @SmokeTest
    @Test
    fun openAllTabsFromACollectionTest() {
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            clickCollectionThreeDotButton(composeTestRule)
            selectOpenTabs(composeTestRule)
        }
        tabDrawer {
            verifyExistingOpenTabs(firstTestPage.title, secondTestPage.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/343426
    @SmokeTest
    @Test
    fun shareAllTabsFromACollectionTest() {
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
        }.clickShareCollectionButton {
            verifyShareTabsOverlay(firstWebsite.title, secondWebsite.title)
            verifySharingWithSelectedApp(sharingApp, urlString, collectionName)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/343428
    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            clickCollectionThreeDotButton(composeTestRule)
            selectDeleteCollection(composeTestRule)
        }

        homeScreen {
            verifySnackBarText("Collection deleted")
            clickSnackbarButton("UNDO")
            verifyCollectionIsDisplayed(collectionName, true)
        }

        homeScreen {
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            clickCollectionThreeDotButton(composeTestRule)
            selectDeleteCollection(composeTestRule)
        }

        homeScreen {
            verifySnackBarText("Collection deleted")
            verifyNoCollectionsText()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2319453
    // open a webpage, and add currently opened tab to existing collection
    @Test
    fun saveTabToExistingCollectionFromMainMenuTest() {
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(firstWebPage.title)
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    // Testrail link: https://testrail.stage.mozaws.net/index.php?/cases/view/343423
    @Test
    fun saveTabToExistingCollectionUsingTheAddTabButtonTest() {
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            clickCollectionThreeDotButton(composeTestRule)
            selectAddTabToCollection(composeTestRule)
            verifyTabsSelectedCounterText(1)
            saveTabsSelectedForCollection()
            verifySnackBarText("Tab saved!")
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/343424
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
            verifyCollectionIsDisplayed(firstCollectionName)
        }.expandCollection(firstCollectionName) {
            clickCollectionThreeDotButton(composeTestRule)
            selectRenameCollection(composeTestRule)
        }.typeCollectionNameAndSave(secondCollectionName) {}

        homeScreen {
            verifyCollectionIsDisplayed(secondCollectionName)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/991248
    @Test
    fun createCollectionUsingSelectTabsButtonTest() {
        val firstWebPage = getGenericAsset(mockWebServer, 1)
        val secondWebPage = getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
        }.openTabDrawer {
            createCollection(
                tabTitles = arrayOf(firstWebPage.title, secondWebPage.title),
                collectionName = firstCollectionName,
            )
            verifySnackBarText("Collection saved!")
        }.closeTabDrawer {
        }.goToHomescreen {
            verifyCollectionIsDisplayed(firstCollectionName)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/2319455
    @Test
    fun removeTabFromCollectionUsingTheCloseButtonTest() {
        val webPage = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(webPage.url) {
        }.openTabDrawer {
            createCollection(webPage.title, collectionName = collectionName)
            closeTab()
        }

        homeScreen {
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(webPage.title, true)
            removeTabFromCollection(webPage.title)
        }
        homeScreen {
            verifySnackBarText("Collection deleted")
            clickSnackbarButton("UNDO")
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(webPage.title, true)
            removeTabFromCollection(webPage.title)
            verifyTabSavedInCollection(webPage.title, false)
        }
        homeScreen {
            verifyCollectionIsDisplayed(collectionName, false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/343427
    @Test
    fun removeTabFromCollectionUsingSwipeLeftActionTest() {
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            swipeTabLeft(testPage.title, composeTestRule)
            verifyTabSavedInCollection(testPage.title, false)
        }
        homeScreen {
            verifySnackBarText("Collection deleted")
            clickSnackbarButton("UNDO")
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(testPage.title, true)
            swipeTabLeft(testPage.title, composeTestRule)
            verifyTabSavedInCollection(testPage.title, false)
        }
        homeScreen {
            verifyCollectionIsDisplayed(collectionName, false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/991278
    @Test
    fun removeTabFromCollectionUsingSwipeRightActionTest() {
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
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            swipeTabRight(testPage.title, composeTestRule)
            verifyTabSavedInCollection(testPage.title, false)
        }
        homeScreen {
            verifySnackBarText("Collection deleted")
            clickSnackbarButton("UNDO")
            verifyCollectionIsDisplayed(collectionName)
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(testPage.title, true)
            swipeTabRight(testPage.title, composeTestRule)
            verifyTabSavedInCollection(testPage.title, false)
        }
        homeScreen {
            verifyCollectionIsDisplayed(collectionName, false)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/991276
    @Test
    fun createCollectionByLongPressingOpenTabsTest() {
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
        }.expandCollection(collectionName) {
            verifyTabSavedInCollection(firstWebPage.title)
            verifyTabSavedInCollection(secondWebPage.title)
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/344897
    @Test
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
}
