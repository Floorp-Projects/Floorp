/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits

import io.mockk.coVerify
import io.mockk.mockk
import io.mockk.slot
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataObservation
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.historymetadata.DefaultHistoryMetadataService
import org.mozilla.fenix.historymetadata.HistoryMetadataService

class HistoryMetadataServiceTest {

    private lateinit var service: HistoryMetadataService
    private lateinit var storage: HistoryMetadataStorage

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Before
    fun setup() {
        storage = mockk(relaxed = true)
        service = DefaultHistoryMetadataService(storage, scope)
    }

    @Test
    fun `GIVEN a regular page WHEN metadata is created THEN a regular document type observation is recorded`() = runTestOnMain {
        val parent = createTab("https://mozilla.org")
        val tab = createTab("https://blog.mozilla.org", parent = parent)
        service.createMetadata(tab, searchTerms = "hello", referrerUrl = parent.content.url)
        advanceUntilIdle()

        val expectedKey = HistoryMetadataKey(url = tab.content.url, searchTerm = "hello", referrerUrl = parent.content.url)
        val expectedObservation = HistoryMetadataObservation.DocumentTypeObservation(documentType = DocumentType.Regular)
        coVerify { storage.noteHistoryMetadataObservation(expectedKey, expectedObservation) }
    }

    @Test
    fun `GIVEN a media page WHEN metadata is created THEN a media document type observation is recorded`() = runTestOnMain {
        val tab = createTab("https://media.mozilla.org", mediaSessionState = mockk())
        service.createMetadata(tab)
        advanceUntilIdle()

        val expectedKey = HistoryMetadataKey(url = tab.content.url)
        val expectedObservation = HistoryMetadataObservation.DocumentTypeObservation(documentType = DocumentType.Media)
        coVerify { storage.noteHistoryMetadataObservation(expectedKey, expectedObservation) }
    }

    @Test
    fun `GIVEN existing metadata WHEN metadata is created THEN correct document type observation is recorded`() = runTestOnMain {
        val existingKey = HistoryMetadataKey(url = "https://media.mozilla.org", referrerUrl = "https://mozilla.org")
        val tab = createTab("https://media.mozilla.org", historyMetadata = existingKey)
        service.createMetadata(tab)
        advanceUntilIdle()

        var expectedKey = HistoryMetadataKey(url = tab.content.url, referrerUrl = existingKey.referrerUrl)
        var expectedObservation = HistoryMetadataObservation.DocumentTypeObservation(documentType = DocumentType.Regular)
        coVerify { storage.noteHistoryMetadataObservation(expectedKey, expectedObservation) }

        val otherTab = createTab("https://blog.mozilla.org", historyMetadata = existingKey)
        service.createMetadata(otherTab)
        advanceUntilIdle()

        expectedKey = HistoryMetadataKey(url = otherTab.content.url)
        expectedObservation = HistoryMetadataObservation.DocumentTypeObservation(documentType = DocumentType.Regular)
        coVerify { storage.noteHistoryMetadataObservation(expectedKey, expectedObservation) }
    }

    @Test
    fun `WHEN metadata is updated THEN a view time observation is recorded`() = runTestOnMain {
        val now = System.currentTimeMillis()
        val key = HistoryMetadataKey(url = "https://blog.mozilla.org")
        val tab = createTab(key.url, historyMetadata = key, lastAccess = now - 60 * 1000)
        service.updateMetadata(key, tab)
        advanceUntilIdle()

        val observation = slot<HistoryMetadataObservation.ViewTimeObservation>()
        coVerify { storage.noteHistoryMetadataObservation(key, capture(observation)) }
        assertTrue(observation.captured.viewTime >= 60 * 1000)
    }

    @Test
    fun `WHEN metadata is updated for a tab with no lastAccess THEN view time observation is not recorded`() = runTestOnMain {
        val key = HistoryMetadataKey(url = "https://blog.mozilla.org")
        val tab = createTab(key.url, historyMetadata = key, lastAccess = 0)
        service.updateMetadata(key, tab)
        advanceUntilIdle()

        coVerify(exactly = 0) { storage.noteHistoryMetadataObservation(key, any()) }
    }

    @Test
    fun `WHEN metadata is updated for a tab with unchanged lastAccess THEN view time observation is not recorded`() = runTestOnMain {
        val now = System.currentTimeMillis()
        val key = HistoryMetadataKey(url = "https://blog.mozilla.org")
        val tab = createTab(key.url, historyMetadata = key, lastAccess = now - 60 * 1000)
        service.updateMetadata(key, tab)
        advanceUntilIdle()

        val observation = slot<HistoryMetadataObservation.ViewTimeObservation>()
        coVerify(exactly = 1) { storage.noteHistoryMetadataObservation(key, capture(observation)) }
        assertTrue(observation.captured.viewTime >= 60 * 1000)

        // Now, call update again with the same lastAccess value. Storage method won't be hit again.
        service.updateMetadata(key, tab)
        advanceUntilIdle()
        coVerify(exactly = 1) { storage.noteHistoryMetadataObservation(key, any()) }
    }

    @Test
    fun `WHEN cleanup is called THEN old metadata is deleted`() = runTestOnMain {
        val timestamp = System.currentTimeMillis() - 7 * 24 * 60 * 60 * 1000
        service.cleanup(timestamp)
        advanceUntilIdle()

        coVerify { storage.deleteHistoryMetadataOlderThan(timestamp) }
    }
}
