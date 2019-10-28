/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.NoMatchingViewException
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import okhttp3.mockwebserver.MockWebServer
import org.junit.Ignore
import org.junit.Before
import org.junit.After
import org.junit.Test
import org.junit.Rule
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying basic functionality of tab collection
 *
 */

class CollectionTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    private val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule()

    @Before
    fun setUp() {
        mockWebServer = MockWebServer().apply {
            setDispatcher(AndroidAssetDispatcher())
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    @Ignore("Temp disable test - see: https://github.com/mozilla-mobile/fenix/issues/5793")
    // open a webpage, and add currently opened tab to existing collection
    fun addTabToCollectionTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        createCollection(firstWebPage.url, "testcollection_1")

        // Close the open tab
        homeScreen {
            verifyHomeScreen()
        }
        onView(ViewMatchers.withId(R.id.close_tab_button)).click()

        // On homeview, open another webpage
        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            verifyPageContent(secondWebPage.content)
        }

        // Save the current page to the testcollection_1
        navigationToolbar {
        }.openThreeDotMenu {
            // click save to collection menu item, type collection name
            clickBrowserViewSaveCollectionButton()
            org.mozilla.fenix.ui.robots.mDevice.wait(
                Until.findObject(By.text("testcollection_1")),
                TestAssetHelper.waitingTime)
            onView(ViewMatchers.withText("testcollection_1")).click()
            mDevice.pressBack() // go to main page
        }

        // close currently opened tab
        homeScreen {
            verifyHomeScreen()
            onView(ViewMatchers.withId(R.id.close_tab_button)).click()
            org.mozilla.fenix.ui.robots.mDevice.wait(
                Until.findObject(By.text("testcollection_1")),
                TestAssetHelper.waitingTime)
            // On homeview, expand the collection and open the first saved page
            onView(ViewMatchers.withText("testcollection_1")).click()
            onView(ViewMatchers.withText("Test_Page_1")).click()
        }
        // Page content: 1
        browserScreen {
            verifyPageContent("Page content: 1")
            mDevice.pressBack() // go to main page
        }

        // tab_in_collection_item
        homeScreen {
            verifyHomeScreen()
            onView(ViewMatchers.withId(R.id.close_tab_button)).click()
            // On homeview, expand the collection and open the first saved page
            org.mozilla.fenix.ui.robots.mDevice.wait(
                Until.findObject(By.text("Test_Page_2")),
                TestAssetHelper.waitingTime)
            onView(ViewMatchers.withText("Test_Page_2")).click()
        }

        // Page content: 2
        browserScreen {
            verifyPageContent("Page content: 2")
        }
    }

    @Test
    @Ignore("Temp disable test - see: https://github.com/mozilla-mobile/fenix/issues/5793")
    // Rename Collection from the Homescreen
    fun renameCollectionTest() {

        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        createCollection(firstWebPage.url, "testcollection_1")

        homeScreen {
            // On homeview, tap the 3-dot button to expand, select rename, rename collection
            clickCollectionThreeDotButton()
            selectRenameCollection()
            typeCollectionName("renamed_collection")
            mDevice.wait(Until.findObject(By.text("renamed_collection")), TestAssetHelper.waitingTime)
            // Verify the new name is displayed on homeview
            onView(ViewMatchers.withText("renamed_collection"))
                .check(ViewAssertions
                    .matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        }
    }

    @Test
    @Ignore("Temp disable test - see: https://github.com/mozilla-mobile/fenix/issues/5793")
    // Delete Collection from the Homescreen
    fun deleteCollectionTest() {

        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        createCollection(firstWebPage.url, "testcollection_1")

        homeScreen {
            // Choose delete collection from homeview, and confirm
            clickCollectionThreeDotButton()
            selectDeleteCollection()
            confirmDeleteCollection()

            // Check for No collections caption
            onView(ViewMatchers.withText("No collections"))
                .check(ViewAssertions
                    .matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        }
    }

    @Test
    @Ignore("Temp disable test - see: https://github.com/mozilla-mobile/fenix/issues/5793")
    fun createCollectionTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        createCollection(firstWebPage.url, "testcollection_1")
        createCollection(secondWebPage.url, "testcollection_2", false)

        // On the main screen, swipe to bottom until the collections are shown
        homeScreen {
            // swipe to bottom until the collections are shown
            verifyHomeScreen()
            try {
                onView(ViewMatchers.withText("testcollection_1"))
                    .check(ViewAssertions
                        .matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
            } catch (e: NoMatchingViewException) {
                scrollToElementByText("testcollection_1")
            }
            onView(ViewMatchers.withText("testcollection_2")).check(ViewAssertions
                .matches(ViewMatchers.withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        }
    }

    private fun createCollection(url: Uri, collectionName: String, firstCollection: Boolean = true) {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        // Open a webpage and save to collection "testcollection_1"
        navigationToolbar {
        }.enterURLAndEnterToBrowser(url) {
            verifyPageContent(firstWebPage.content)
        }
        navigationToolbar {
        }.openThreeDotMenu {
            // click save to collection menu item, type collection name
            clickBrowserViewSaveCollectionButton()
            if (!firstCollection)
                clickAddNewCollection()

        }.typeCollectionName(collectionName) {}

        mDevice.pressBack() // go to main page
        org.mozilla.fenix.ui.robots.mDevice.wait(
            Until.findObject(By.text(collectionName)),
            TestAssetHelper.waitingTime)
    }
}
