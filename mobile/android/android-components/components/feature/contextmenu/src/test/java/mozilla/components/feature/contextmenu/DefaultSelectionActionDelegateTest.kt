package mozilla.components.feature.contextmenu

import android.content.res.Resources
import mozilla.components.feature.search.SearchAdapter
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.anyString
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class DefaultSelectionActionDelegateTest {

    var lambdaValue: String? = null
    val shareClicked: (String) -> Unit = { lambdaValue = it }

    @Test
    fun `are non-private actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(false)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources(),
            shareClicked
        )

        assertTrue(delegate.isActionAvailable(SEARCH))
        assertTrue(delegate.isActionAvailable(SHARE))
        assertFalse(delegate.isActionAvailable(SEARCH_PRIVATELY))
    }

    @Test
    fun `are non-private non-share actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(false)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources()
        )

        assertTrue(delegate.isActionAvailable(SEARCH))
        assertFalse(delegate.isActionAvailable(SHARE))
        assertFalse(delegate.isActionAvailable(SEARCH_PRIVATELY))
    }

    @Test
    fun `are private actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(true)
        }
        val delegate = DefaultSelectionActionDelegate(
            searchAdapter,
            getTestResources(),
            shareClicked
        )

        assertTrue(delegate.isActionAvailable(SEARCH_PRIVATELY))
        assertTrue(delegate.isActionAvailable(SHARE))
        assertFalse(delegate.isActionAvailable(SEARCH))
    }

    @Test
    fun `when share ID is passed to perform action it should invoke the lambda`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction(SHARE, "some selected text")

        assertEquals(lambdaValue, "some selected text")
    }

    @Test
    fun `when unknown ID is passed to performAction it should not perform a search`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction("unrecognized string", "some selected text")

        verify(adapter, times(0)).sendSearch(anyBoolean(), anyString())
    }

    @Test
    fun `when unknown ID is passed to performAction it not consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        val result = delegate.performAction("unrecognized string", "some selected text")

        assertFalse(result)
    }

    @Test
    fun `when search ID is passed to performAction it should perform a search`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction(SEARCH, "some selected text")

        verify(adapter, times(1)).sendSearch(false, "some selected text")
    }

    @Test
    fun `when search ID is passed to performAction it should consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        val result = delegate.performAction(SEARCH, "some selected text")

        assertTrue(result)
    }

    @Test
    fun `when private search ID is passed to performAction it should perform a private normal search`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        delegate.performAction(SEARCH_PRIVATELY, "some selected text")

        verify(adapter, times(1)).sendSearch(true, "some selected text")
    }

    @Test
    fun `when private search ID is passed to performAction it should consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate =
            DefaultSelectionActionDelegate(adapter, getTestResources(), shareClicked)

        val result = delegate.performAction(SEARCH_PRIVATELY, "some selected text")

        assertTrue(result)
    }
}

fun getTestResources() = mock<Resources> {
    whenever(getString(R.string.mozac_selection_context_menu_search_2)).thenReturn("Search")
    whenever(getString(R.string.mozac_selection_context_menu_search_privately_2))
        .thenReturn("search privately")
    whenever(getString(R.string.mozac_selection_context_menu_share)).thenReturn("share")
}
