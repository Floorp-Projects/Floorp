package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryListGroupBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.viewholders.HistoryGroupViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class HistoryGroupViewHolderTest {

    private lateinit var binding: HistoryListGroupBinding
    private lateinit var interactor: HistoryInteractor

    private val metaDataItem = History.Metadata(
        position = 0,
        title = "Mozilla",
        url = "https://foundation.mozilla.org",
        visitedAt = 12398410293L,
        historyTimeGroup = HistoryItemTimeGroup.Today,
        totalViewTime = 1250,
        historyMetadataKey = HistoryMetadataKey(
            url = "https://foundation.mozilla.org",
            searchTerm = "mozilla"
        ),
        selected = false
    )

    private val historyGroupItem = HistoryViewItem.HistoryGroupItem(
        data = History.Group(
            position = 0,
            title = "Mozilla",
            visitedAt = 12398410293L,
            historyTimeGroup = HistoryItemTimeGroup.Today,
            items = listOf(
                metaDataItem,
                metaDataItem.copy(position = 1),
                metaDataItem.copy(position = 2),
                metaDataItem.copy(position = 3)
            ),
            selected = false
        )
    )

    @Before
    fun setup() {
        binding = HistoryListGroupBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN a history group item has more than one item THEN viewHolder uses text for multiple sites`() {
        val viewHolder = testViewHolder()

        val expectedText = String.format(testContext.resources.getString(R.string.history_search_group_sites), 5)
        val actualText = viewHolder.getGroupCountText(5, 0, testContext.resources)
        assertEquals(expectedText, actualText)
    }

    @Test
    fun `GIVEN a history group item has exactly one item THEN get text for single site`() {
        val viewHolder = testViewHolder()

        val expectedText = String.format(testContext.resources.getString(R.string.history_search_group_site), 1)
        val actualText = viewHolder.getGroupCountText(1, 0, testContext.resources)
        assertEquals(expectedText, actualText)
    }

    @Test
    fun `GIVEN a new history group item on bind THEN set the history group name and items size`() {
        val viewHolder = testViewHolder()
        viewHolder.bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 0)

        val childrenSizeExpectedText = viewHolder.getGroupCountText(
            historyGroupItem.data.items.size,
            0,
            testContext.resources
        )
        assertEquals(historyGroupItem.data.title, binding.historyGroupLayout.titleView.text)
        assertEquals(childrenSizeExpectedText, binding.historyGroupLayout.urlView.text)
    }

    @Test
    fun `GIVEN pending deletion not zero THEN adjust items size`() {
        val viewHolder = testViewHolder()
        viewHolder.bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 1)

        val childrenSizeExpectedText = viewHolder.getGroupCountText(
            historyGroupItem.data.items.size,
            1,
            testContext.resources
        )
        assertEquals(childrenSizeExpectedText, binding.historyGroupLayout.urlView.text)
    }

    @Test
    fun `WHEN a history item delete icon is clicked THEN onDeleteClicked is called`() {
        var isDeleteClicked = false
        testViewHolder(
            onDeleteClicked = { isDeleteClicked = true }
        ).bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 0)

        binding.historyGroupLayout.overflowView.performClick()
        assertEquals(true, isDeleteClicked)
    }

    @Test
    fun `WHEN a history item is clicked THEN interactor open is called`() {
        testViewHolder().bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 0)

        binding.historyGroupLayout.performClick()
        verify { interactor.open(historyGroupItem.data) }
    }

    @Test
    fun `GIVEN selecting mode THEN delete button is not visible `() {
        testViewHolder().bind(historyGroupItem, HistoryFragmentState.Mode.Editing(setOf()), 0)

        assertEquals(View.INVISIBLE, binding.historyGroupLayout.overflowView.visibility)
    }

    @Test
    fun `GIVEN normal mode THEN delete button is visible `() {
        testViewHolder().bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 0)

        assertEquals(View.VISIBLE, binding.historyGroupLayout.overflowView.visibility)
    }

    @Test
    fun `GIVEN editing mode WHEN item is selected THEN checkmark is visible `() {
        testViewHolder(
            selectedHistoryItems = setOf(historyGroupItem.data)
        ).bind(historyGroupItem, HistoryFragmentState.Mode.Editing(setOf(historyGroupItem.data)), 0)

        assertEquals(1, binding.historyGroupLayout.binding.icon.displayedChild)
    }

    @Test
    fun `GIVEN editing mode WHEN item is not selected THEN checkmark is not visible `() {
        testViewHolder().bind(historyGroupItem, HistoryFragmentState.Mode.Editing(setOf()), 0)

        assertEquals(0, binding.historyGroupLayout.binding.icon.displayedChild)
    }

    @Test
    fun `GIVEN normal mode WHEN item is long pressed THEN interactor select is called`() {
        testViewHolder().bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 0)

        binding.historyGroupLayout.performLongClick()
        verify { interactor.select(historyGroupItem.data) }
    }

    @Test
    fun `GIVEN editing mode and item is not selected WHEN item is clicked THEN interactor select is called`() {
        testViewHolder(
            selectedHistoryItems = setOf(historyGroupItem.data.copy(position = 1))
        ).bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 0)

        binding.historyGroupLayout.performClick()
        verify { interactor.select(historyGroupItem.data) }
    }

    @Test
    fun `GIVEN editing mode and item is selected WHEN item is clicked THEN interactor select is called`() {
        testViewHolder(
            selectedHistoryItems = setOf(historyGroupItem.data)
        ).bind(historyGroupItem, HistoryFragmentState.Mode.Normal, 0)

        binding.historyGroupLayout.performClick()
        verify { interactor.deselect(historyGroupItem.data) }
    }

    private fun testViewHolder(
        view: View = binding.root,
        historyInteractor: HistoryInteractor = interactor,
        selectedHistoryItems: Set<History> = setOf(),
        onDeleteClicked: (Int) -> Unit = {}
    ): HistoryGroupViewHolder {
        return HistoryGroupViewHolder(
            view = view,
            historyInteractor = historyInteractor,
            selectionHolder = mockk { every { selectedItems } returns selectedHistoryItems },
            onDeleteClicked = onDeleteClicked
        )
    }
}
