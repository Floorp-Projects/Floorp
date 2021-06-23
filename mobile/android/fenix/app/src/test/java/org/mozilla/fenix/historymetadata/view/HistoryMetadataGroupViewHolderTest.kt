/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.view

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.history_metadata_group.view.*
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.historymetadata.interactor.HistoryMetadataInteractor

@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataGroupViewHolderTest {

    private lateinit var view: View
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
        view = LayoutInflater.from(testContext).inflate(HistoryMetadataGroupViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN a history metadata group on bind THEN set the title text and isActivated state`() {
        HistoryMetadataGroupViewHolder(view, interactor).bind(historyGroup)

        assertEquals(historyGroup.title, view.history_metadata_group_title.text)
        assertEquals(historyGroup.expanded, view.isActivated)
    }

    @Test
    fun `WHEN a history metadata group is clicked THEN interactor is called`() {
        HistoryMetadataGroupViewHolder(view, interactor).bind(historyGroup)

        view.performClick()

        verify { interactor.onToggleHistoryMetadataGroupExpanded(historyGroup) }
    }
}
