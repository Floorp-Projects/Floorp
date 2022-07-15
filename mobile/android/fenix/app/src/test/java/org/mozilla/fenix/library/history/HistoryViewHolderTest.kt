package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.HistoryListHistoryBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.viewholders.HistoryViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class HistoryViewHolderTest {

    private lateinit var binding: HistoryListHistoryBinding
    private lateinit var interactor: HistoryInteractor
    private lateinit var iconsLoader: BrowserIcons

    private val historyItem = HistoryViewItem.HistoryItem(
        data = History.Regular(
            position = 0,
            title = "Mozilla",
            url = "https://foundation.mozilla.org",
            visitedAt = 12398410293L,
            historyTimeGroup = HistoryItemTimeGroup.Today,
            selected = false
        )
    )

    @Before
    fun setup() {
        binding = HistoryListHistoryBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
        iconsLoader = spyk(
            BrowserIcons(
                testContext,
                mockk(relaxed = true)
            )
        )
        every { testContext.components.core.icons } returns iconsLoader
    }

    @Test
    fun `GIVEN a new history item on bind THEN set the history title and url text`() {
        testViewHolder().bind(historyItem, HistoryFragmentState.Mode.Normal)

        assertEquals(historyItem.data.title, binding.historyLayout.titleView.text)
        assertEquals(historyItem.data.url, binding.historyLayout.urlView.text)
    }

    @Test
    fun `WHEN a new history item on bind THEN the icon is loaded`() {
        testViewHolder(
            selectedHistoryItems = setOf(historyItem.data)
        ).bind(historyItem, HistoryFragmentState.Mode.Editing(setOf(historyItem.data)))

        verify { iconsLoader.loadIntoView(binding.historyLayout.iconView, historyItem.data.url) }
    }

    @Test
    fun `WHEN the same history item on bind twice THEN the icon is not loaded again`() {
        testViewHolder(
            selectedHistoryItems = setOf(historyItem.data)
        ).apply {
            bind(historyItem, HistoryFragmentState.Mode.Editing(setOf(historyItem.data)))
            bind(historyItem, HistoryFragmentState.Mode.Editing(setOf(historyItem.data)))
        }

        verify(exactly = 1) {
            iconsLoader.loadIntoView(
                binding.historyLayout.iconView,
                historyItem.data.url
            )
        }
    }

    @Test
    fun `WHEN a history item delete icon is clicked THEN onDeleteClicked is called`() {
        var isDeleteClicked = false
        testViewHolder(
            onDeleteClicked = { isDeleteClicked = true }
        ).bind(historyItem, HistoryFragmentState.Mode.Normal)

        binding.historyLayout.overflowView.performClick()
        assertEquals(true, isDeleteClicked)
    }

    @Test
    fun `WHEN a history item is clicked THEN interactor open is called`() {
        testViewHolder().bind(historyItem, HistoryFragmentState.Mode.Normal)

        binding.historyLayout.performClick()
        verify { interactor.open(historyItem.data) }
    }

    @Test
    fun `GIVEN selecting mode THEN delete button is not visible `() {
        testViewHolder().bind(historyItem, HistoryFragmentState.Mode.Editing(setOf()))

        assertEquals(View.INVISIBLE, binding.historyLayout.overflowView.visibility)
    }

    @Test
    fun `GIVEN normal mode THEN delete button is visible `() {
        testViewHolder().bind(historyItem, HistoryFragmentState.Mode.Normal)

        assertEquals(View.VISIBLE, binding.historyLayout.overflowView.visibility)
    }

    @Test
    fun `GIVEN editing mode WHEN item is selected THEN checkmark is visible `() {
        testViewHolder(
            selectedHistoryItems = setOf(historyItem.data)
        ).bind(historyItem, HistoryFragmentState.Mode.Editing(setOf(historyItem.data)))

        assertEquals(1, binding.historyLayout.binding.icon.displayedChild)
    }

    @Test
    fun `GIVEN editing mode WHEN item is not selected THEN checkmark is not visible `() {
        testViewHolder().bind(historyItem, HistoryFragmentState.Mode.Editing(setOf()))

        assertEquals(0, binding.historyLayout.binding.icon.displayedChild)
    }

    @Test
    fun `GIVEN normal mode WHEN item is long pressed THEN interactor select is called`() {
        testViewHolder().bind(historyItem, HistoryFragmentState.Mode.Normal)

        binding.historyLayout.performLongClick()
        verify { interactor.select(historyItem.data) }
    }

    @Test
    fun `GIVEN editing mode and item is not selected WHEN item is clicked THEN interactor select is called`() {
        testViewHolder(
            selectedHistoryItems = setOf(historyItem.data.copy(position = 1))
        ).bind(historyItem, HistoryFragmentState.Mode.Normal)

        binding.historyLayout.performClick()
        verify { interactor.select(historyItem.data) }
    }

    @Test
    fun `GIVEN editing mode and item is selected WHEN item is clicked THEN interactor select is called`() {
        testViewHolder(
            selectedHistoryItems = setOf(historyItem.data)
        ).bind(historyItem, HistoryFragmentState.Mode.Normal)

        binding.historyLayout.performClick()
        verify { interactor.deselect(historyItem.data) }
    }

    private fun testViewHolder(
        view: View = binding.root,
        historyInteractor: HistoryInteractor = interactor,
        selectedHistoryItems: Set<History> = setOf(),
        onDeleteClicked: (Int) -> Unit = {}
    ): HistoryViewHolder {
        return HistoryViewHolder(
            view = view,
            historyInteractor = historyInteractor,
            selectionHolder = mockk { every { selectedItems } returns selectedHistoryItems },
            onDeleteClicked = onDeleteClicked
        )
    }
}
