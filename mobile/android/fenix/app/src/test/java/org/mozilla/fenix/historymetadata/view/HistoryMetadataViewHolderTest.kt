/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.view

import android.view.LayoutInflater
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.HistoryMetadataListRowBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataViewHolderTest {

    private lateinit var binding: HistoryMetadataListRowBinding
    private lateinit var interactor: SessionControlInteractor
    private lateinit var icons: BrowserIcons

    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com", null, null),
        title = "mozilla",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = DocumentType.Regular
    )

    @Before
    fun setup() {
        binding = HistoryMetadataListRowBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)

        icons = mockk(relaxed = true)

        every { icons.loadIntoView(binding.historyMetadataIcon, any()) } returns mockk()
    }

    @Test
    fun `GIVEN a history metadata on bind THEN set the title text and load the tab icon`() {
        HistoryMetadataViewHolder(binding.root, interactor, icons).bind(historyEntry)

        assertEquals(historyEntry.title, binding.historyMetadataTitle.text)

        verify { icons.loadIntoView(binding.historyMetadataIcon, IconRequest(historyEntry.key.url)) }
    }

    @Test
    fun `WHEN a history metadata item is clicked THEN interactor is called`() {
        HistoryMetadataViewHolder(binding.root, interactor, icons).bind(historyEntry)

        binding.root.performClick()

        verify { interactor.onHistoryMetadataItemClicked(historyEntry.key.url, historyEntry.key) }
    }

    @Test
    fun `WHEN a recent tab does not have a title THEN show the url`() {
        val historyEntryWithoutTitle = HistoryMetadata(
            key = HistoryMetadataKey("http://www.mozilla.com", null, null),
            title = null,
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular
        )

        HistoryMetadataViewHolder(binding.root, interactor, icons).bind(historyEntryWithoutTitle)

        assertEquals(historyEntry.key.url, binding.historyMetadataTitle.text)
    }
}
