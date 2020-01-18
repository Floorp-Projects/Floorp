package mozilla.components.feature.contextmenu

import android.content.res.Resources
import mozilla.components.feature.search.SearchAdapter
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.anyString
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

private const val APP_NAME = "appName"

class DefaultSelectionActionDelegateTest {

    @Test
    fun `are non-private actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(false)
        }
        val delegate = DefaultSelectionActionDelegate(searchAdapter, getTestResources(), APP_NAME)

        assertTrue(delegate.isActionAvailable(SEARCH))
        assertFalse(delegate.isActionAvailable(SEARCH_PRIVATELY))
    }

    @Test
    fun `are private actions available`() {
        val searchAdapter = mock<SearchAdapter> {
            whenever(isPrivateSession()).thenReturn(true)
        }
        val delegate = DefaultSelectionActionDelegate(searchAdapter, getTestResources(), APP_NAME)

        assertTrue(delegate.isActionAvailable(SEARCH_PRIVATELY))
        assertFalse(delegate.isActionAvailable(SEARCH))
    }

    @Test
    fun `when search ID is passed to performAction it should not perform a search`() {
        val adapter = mock<SearchAdapter>()
        val delegate = DefaultSelectionActionDelegate(adapter, getTestResources(), APP_NAME)

        delegate.performAction("unrecognized string", "some selected text")

        verify(adapter, times(0)).sendSearch(anyBoolean(), anyString())
    }

    @Test
    fun `when unknown ID is passed to performAction it not consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate = DefaultSelectionActionDelegate(adapter, getTestResources(), APP_NAME)

        val result = delegate.performAction("unrecognized string", "some selected text")

        assertFalse(result)
    }

    @Test
    fun `when search ID is passed to performAction it should perform a search`() {
        val adapter = mock<SearchAdapter>()
        val delegate = DefaultSelectionActionDelegate(adapter, getTestResources(), APP_NAME)

        delegate.performAction(SEARCH, "some selected text")

        verify(adapter, times(1)).sendSearch(false, "some selected text")
    }

    @Test
    fun `when search ID is passed to performAction it should consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate = DefaultSelectionActionDelegate(adapter, getTestResources(), APP_NAME)

        val result = delegate.performAction(SEARCH, "some selected text")

        assertTrue(result)
    }

    @Test
    fun `when private search ID is passed to performAction it should perform a private normal search`() {
        val adapter = mock<SearchAdapter>()
        val delegate = DefaultSelectionActionDelegate(adapter, getTestResources(), APP_NAME)

        delegate.performAction(SEARCH_PRIVATELY, "some selected text")

        verify(adapter, times(1)).sendSearch(true, "some selected text")
    }

    @Test
    fun `when private search ID is passed to performAction it should consume the action`() {
        val adapter = mock<SearchAdapter>()
        val delegate = DefaultSelectionActionDelegate(adapter, getTestResources(), APP_NAME)

        val result = delegate.performAction(SEARCH_PRIVATELY, "some selected text")

        assertTrue(result)
    }
}

fun getTestResources() = mock<Resources> {
    whenever(getString(R.string.mozac_selection_context_menu_search, APP_NAME)).thenReturn("search")
    whenever(getString(R.string.mozac_selection_context_menu_search_privately, APP_NAME)).thenReturn("search privately")
}
