package org.mozilla.fenix.search.toolbar

import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SearchSelectorMenuTest {

    private lateinit var menu: SearchSelectorMenu
    private val interactor = mockk<ToolbarInteractor>()

    @Before
    fun setup() {
        menu = SearchSelectorMenu(testContext, interactor)
    }

    @Test
    fun `WHEN building the menu items THEN the header is the first item AND the search settings is the last item`() {
        every { interactor.onMenuItemTapped(any()) } just Runs

        val items = menu.menuItems(listOf())
        val lastItem = (items.last() as TextMenuCandidate)
        lastItem.onClick()

        assertEquals(
            testContext.getString(R.string.search_header_menu_item_2),
            (items.first() as DecorativeTextMenuCandidate).text,
        )
        assertEquals(
            testContext.getString(R.string.search_settings_menu_item),
            lastItem.text,
        )
        verify { interactor.onMenuItemTapped(SearchSelectorMenu.Item.SearchSettings) }
    }
}
