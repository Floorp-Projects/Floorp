/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.library.historymetadata.view

import android.view.LayoutInflater
import androidx.navigation.Navigation
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.HistoryMetadataGroupListItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.library.historymetadata.interactor.HistoryMetadataGroupInteractor
import org.mozilla.fenix.selection.SelectionHolder

@RunWith(FenixRobolectricTestRunner::class)
class HistoryMetadataGroupItemViewHolderTest {

    private lateinit var binding: HistoryMetadataGroupListItemBinding
    private lateinit var interactor: HistoryMetadataGroupInteractor
    private lateinit var selectionHolder: SelectionHolder<History.Metadata>

    private val item = History.Metadata(
        position = 1,
        title = "Mozilla",
        url = "mozilla.org",
        visitedAt = 0,
        historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(0),
        totalViewTime = 0,
        historyMetadataKey = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
    )

    @Before
    fun setup() {
        binding = HistoryMetadataGroupListItemBinding.inflate(LayoutInflater.from(testContext))
        Navigation.setViewNavController(binding.root, mockk(relaxed = true))
        interactor = mockk(relaxed = true)
        selectionHolder = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN a history metadata item on bind THEN set the title and url text`() {
        every { testContext.components.core.icons } returns BrowserIcons(testContext, mockk(relaxed = true))
        HistoryMetadataGroupItemViewHolder(binding.root, interactor, selectionHolder).bind(item, isPendingDeletion = false)

        assertEquals(item.title, binding.historyLayout.titleView.text)
        assertEquals(item.url, binding.historyLayout.urlView.text)
    }
}
