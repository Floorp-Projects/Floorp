/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.view

import android.view.LayoutInflater
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.HistoryMetadataGroupBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.historymetadata.interactor.HistoryMetadataInteractor

@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataGroupViewHolderTest {

    private lateinit var binding: HistoryMetadataGroupBinding
    private lateinit var interactor: HistoryMetadataInteractor

    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
        title = "mozilla",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = DocumentType.Regular
    )
    private val historyGroup = HistoryMetadataGroup(
        title = "mozilla",
        historyMetadata = listOf(historyEntry)
    )

    @Before
    fun setup() {
        binding = HistoryMetadataGroupBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN a history metadata group on bind THEN set the title text and isActivated state`() {
        HistoryMetadataGroupViewHolder(binding.root, interactor).bind(historyGroup)

        assertEquals(historyGroup.title, binding.historyMetadataGroupTitle.text)
        assertEquals(historyGroup.expanded, binding.root.isActivated)
    }

    @Test
    fun `WHEN a history metadata group is clicked THEN interactor is called`() {
        HistoryMetadataGroupViewHolder(binding.root, interactor).bind(historyGroup)

        binding.root.performClick()

        verify { interactor.onToggleHistoryMetadataGroupExpanded(historyGroup) }
    }
}
