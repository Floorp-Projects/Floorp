/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.Rule
import org.junit.Before
import org.junit.After
import org.junit.Ignore
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule

/**
 *  Tests for verifying the main three dot menu options
 *
 */

class SettingsPrivacyTest {
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

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun settingsPrivacyItemsTest() {
        // Open 3dot (main) menu
        // Select settings
        // Verify header: "Privacy"

        // Verify item: "Tracking Protection" and default value: "On"
        // Verify item: "Tracking Protection" and default value: "On"

        // Verify item: "Site Permissions"
        // Click on: "Site permissions"
        // Verify sub-menu items...
        // Verify item: Exceptions
        // Verify item: header: "Permissions"
        // Verify item: "Camera" and default value: "ask to allow"
        // Verify item: "Location" and default value: "ask to allow"
        // Verify item: "Microphone" and default value: "ask to allow"
        // Verify item: "Notification" and default value: "ask to allow"

        // Verify item: "Delete browsing data"
        // Click on: "Delete browsing data"
        // Verify sub-menu items...
        // Verify item: "Open tabs"
        // Verify item" <tab count> tabs
        // Verify item: "Browsing history and site data"
        // Verify item" <address count> addresses
        // Verify item:  "Collections
        // Verify item" <collection count> collections
        // Verify item button: "Delete browsing data"

        // Verify item: "Data collection"
        // Click on: "Data collection"
        // Verify sub-menu items...
        // Verify header: "Usage and technical data"
        // Verify description: "Shares performance, usage, hardware and customization data about your browser with Mozilla to help us make Firefox Preview better"
        // Verify item:  toggle default value: 'on'

        // Verify item: "Privacy notice"
        // Verify item: "Leak Canary" and default toggle value: "Off"

    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun toggleTrackingProtection() {
        // Open static test website to verify TP is turned on (default): https://github.com/rpappalax/testapp
        // (static content needs to be migrated to assets folder)
        // Open 3dot (main) menu
        // Select settings
        // Toggle Tracking Protection to 'off'
        // Back arrow to Home
        // Open static test website to verify TP is now off: https://github.com/rpappalax/testapp
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifySitePermissions() {
        // Open 3dot (main) menu
        // Select settings
        // Click on: "Site permissions"
        // Verify sub-menu items...
        // Click on: "Exceptions"
        // Verify: "No site exceptions"
        // TBD: create a site exception
        // TBD: return to this UI and verify

        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: geolocation"
        // Verify that geolocation permissions dialogue is opened
        // Verify text: "Allow <website URL> to use your geolocation?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Don't Allow"
        // Verify button: "Allow" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: geolocation"
        // Verify that geolocation permissions dialogue is not opened
        //
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: camera"
        // Verify that camera permissions dialogue is opened
        // Verify text: "Allow <website URL> to use your camera?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Don't Allow"
        // Verify button: "Allow" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: camera"
        // Verify that camera permissions dialogue is not opened
        //
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: microphone"
        // Verify that microphone permissions dialogue is opened
        // Verify text: "Allow <website URL> to use your microphone?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Don't Allow"
        // Verify button: "Allow" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: microphone"
        // Verify that microphone permissions dialogue is not opened
        //
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: notifications dialogue"
        // Verify that notifications dialogue permissions dialogue is opened
        // Verify text: "Allow <website URL> to send notifications?
        // Verify toggle: 'Remember decision for this site?"
        // Verify button: "Never"
        // Verify button: "Always" (default)
        // Select "Remember decision for this site"
        // Refresh page
        // Click on "Test site permissions: notifications dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //

        // Open 3dot (main) menu
        // Select settings
        // Click on: "Site permissions"
        // Select: Camera
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //
        // Select: Location
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //
        // Select: Microphone
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //
        // Select: Notification
        // Switch from "ask to allow" (default) to "blocked"
        // Click back arrow
        //

        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: camera dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: geolocation dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: microphone dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
        //
        // Open browser to static test website: https://github.com/rpappalax/testapp
        // Click on "Test site permissions: notifications dialogue"
        // Verify that notifications dialogue permissions dialogue is not opened
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun deleteBrowsingData() {
        // Setup:
        // Open 2 websites as 2 tabs
        // Save as 1 collection
        // Open 2 more websites in 2 other tabs
        // Save as a 2nd collection

        // Open 3dot (main) menu
        // Select settings
        // Click on "Delete browsing data"
        // Verify correct number of tabs, addresses and collections are indicated
        // Select all 3 checkboxes
        // Click on "Delete browsing data button"
        // Return to home screen and verify that all tabs, history and collection are gone
        //
        // Verify xxx
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun verifyDataCollection() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Data collection"
        // Verify header: "Usage and technical data"
        // Verify text: "Shares performance, usage, hardware and customization data about your browser with Mozilla"
        //               " to help us make Firefox preview better"
        // Verify toggle is on by default
        // TBD:
        // see: telemetry testcases
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun openPrivacyNotice() {
        // Open 3dot (main) menu
        // Select settings
        // Click on "Privacy notice"
        // Verify redirect to: mozilla.org Privacy notice page"
    }

    @Ignore("This is a stub test, ignore for now")
    @Test
    fun checkLeakCanary() {
        // Open 3dot (main) menu
        // Select settings
        // Click on Leak Canary toggle
        // Verify 'dump' message
    }
}
