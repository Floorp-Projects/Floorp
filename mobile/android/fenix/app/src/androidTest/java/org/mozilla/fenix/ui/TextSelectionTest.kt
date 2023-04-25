package org.mozilla.fenix.ui

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
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.clickContextMenuItem
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.longClickPageObject
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.openEditURLView
import org.mozilla.fenix.ui.robots.searchScreen

class TextSelectionTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityIntentTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

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
    fun selectAllAndCopyTextTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            longClickPageObject(itemContainingText("content"))
            clickContextMenuItem("Select all")
            clickContextMenuItem("Copy")
        }.openNavigationToolbar {
            openEditURLView()
        }

        searchScreen {
            clickClearButton()
            longClickToolbar()
            clickPasteText()
            // With Select all, white spaces are copied
            // Potential bug https://bugzilla.mozilla.org/show_bug.cgi?id=1821310
            verifyTypedToolbarText("  Page content: 1 ")
        }
    }

    @SmokeTest
    @Test
    fun copyTextTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            longClickPageObject(itemContainingText("content"))
            clickContextMenuItem("Copy")
        }.openNavigationToolbar {
            openEditURLView()
        }

        searchScreen {
            clickClearButton()
            longClickToolbar()
            clickPasteText()
            verifyTypedToolbarText("content")
        }
    }

    @SmokeTest
    @Test
    fun shareSelectedTextTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            longClickPageObject(itemWithText(genericURL.content))
        }.clickShareSelectedText {
            verifyAndroidShareLayout()
        }
    }

    @SmokeTest
    @Test
    fun selectAndSearchTextTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            longClickPageObject(itemContainingText("content"))
            clickContextMenuItem("Search")
            mDevice.waitForIdle()
            verifyTabCounter("2")
            verifyUrl("google")
        }
    }

    @SmokeTest
    @Test
    fun privateSelectAndSearchTextTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            longClickPageObject(itemContainingText("content"))
            clickContextMenuItem("Private Search")
            mDevice.waitForIdle()
            verifyTabCounter("2")
            verifyUrl("google")
        }
    }

    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1828663")
    @SmokeTest
    @Test
    fun selectAllAndCopyPDFTextTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF file"))
            longClickPageObject(itemContainingText("Crossing"))
            clickContextMenuItem("Select all")
            clickContextMenuItem("Copy")
        }.openNavigationToolbar {
            openEditURLView()
        }

        searchScreen {
            clickClearButton()
            longClickToolbar()
            clickPasteText()
            // With Select all, white spaces are copied
            // Potential bug https://bugzilla.mozilla.org/show_bug.cgi?id=1821310
            verifyTypedToolbarText(" Washington Crossing the Delaware Wikipedia link ")
        }
    }

    @SmokeTest
    @Test
    fun copyPDFTextTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF file"))
            longClickPageObject(itemContainingText("Crossing"))
            clickContextMenuItem("Copy")
        }.openNavigationToolbar {
            openEditURLView()
        }

        searchScreen {
            clickClearButton()
            longClickToolbar()
            clickPasteText()
            verifyTypedToolbarText("Crossing")
        }
    }

    @SmokeTest
    @Test
    fun shareSelectedPDFTextTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF file"))
            longClickPageObject(itemContainingText("Crossing"))
        }.clickShareSelectedText {
            verifyAndroidShareLayout()
        }
    }

    @SmokeTest
    @Test
    fun selectAndSearchPDFTextTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF file"))
            longClickPageObject(itemContainingText("Crossing"))
            clickContextMenuItem("Search")
            verifyTabCounter("2")
            verifyUrl("google")
        }
    }

    @SmokeTest
    @Test
    fun privateSelectAndSearchPDFTextTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF file"))
            longClickPageObject(itemContainingText("Crossing"))
            clickContextMenuItem("Private Search")
            verifyTabCounter("2")
            verifyUrl("google")
        }
    }
}
