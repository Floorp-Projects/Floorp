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
 *  Tests for verifying basic functionality of history
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

    @Ignore
    @Test
    fun AddTabToCollectionTest() {
        // open a webpage, and add currently opened tab to existing collection
    }

    @Ignore
    @Test
    fun OpenTabFromCollectionTest() {
        // Open one tab from Collection in the Homescreen view
    }

    @Ignore
    @Test
    fun RenameCollectionTest() {
        // Rename Collection from the Homescreen
    }

    @Ignore
    @Test
    fun DeleteCollectionTest() {
        // Delete Collection from the Homescreen
    }

    fun CreateCollection() {
        // Open 3 webpages, and save each of them to a single collection
    }
}
