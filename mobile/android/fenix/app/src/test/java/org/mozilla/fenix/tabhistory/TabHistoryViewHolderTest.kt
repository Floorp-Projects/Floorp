package org.mozilla.fenix.tabhistory

import android.view.View
import io.mockk.CapturingSlot
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibrarySiteItemView

class TabHistoryViewHolderTest {

    @MockK private lateinit var view: View
    @MockK private lateinit var interactor: TabHistoryViewInteractor
    @MockK(relaxed = true) private lateinit var siteItemView: LibrarySiteItemView
    private lateinit var holder: TabHistoryViewHolder
    private lateinit var onClick: CapturingSlot<View.OnClickListener>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        onClick = slot()

        every { siteItemView.setOnClickListener(capture(onClick)) } just Runs
        every { view.findViewById<LibrarySiteItemView>(R.id.history_layout) } returns siteItemView

        holder = TabHistoryViewHolder(view, interactor)
    }

    @Test
    fun `calls interactor on click`() {
        every { interactor.goToHistoryItem(any()) } just Runs

        val item = mockk<TabHistoryItem>(relaxed = true)
        holder.bind(item)
        onClick.captured.onClick(mockk())
        verify { interactor.goToHistoryItem(item) }
    }

    @Test
    fun `binds title and url`() {
        val item = TabHistoryItem(
            title = "Firefox",
            url = "https://firefox.com",
            index = 1,
            isSelected = false
        )
        holder.bind(item)

        verify { siteItemView.displayAs(LibrarySiteItemView.ItemType.SITE) }
        verify { siteItemView.titleView.text = "Firefox" }
        verify { siteItemView.urlView.text = "https://firefox.com" }
        verify { siteItemView.loadFavicon("https://firefox.com") }
    }
}
