/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.res.Resources
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi // for runTest
class SessionSuggestionProviderTest {
    @Test
    fun `Provider returns empty list when text is empty`() = runTest {
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val provider = SessionSuggestionProvider(resources, mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns Sessions with matching URLs`() = runTest {
        val store = BrowserStore()

        val tab1 = createTab("https://www.mozilla.org")
        val tab2 = createTab("https://example.com")
        val tab3 = createTab("https://firefox.com")
        val tab4 = createTab("https://example.org/")

        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val provider = SessionSuggestionProvider(resources, store, mock())

        run {
            val suggestions = provider.onInputChanged("Example")
            assertTrue(suggestions.isEmpty())
        }

        store.dispatch(TabListAction.AddTabAction(tab1)).join()
        store.dispatch(TabListAction.AddTabAction(tab2)).join()
        store.dispatch(TabListAction.AddTabAction(tab3)).join()

        run {
            val suggestions = provider.onInputChanged("Example")
            assertEquals(1, suggestions.size)

            assertEquals(tab2.id, suggestions[0].id)
            assertEquals("Switch to tab", suggestions[0].description)
        }

        store.dispatch(TabListAction.AddTabAction(tab4)).join()

        run {
            val suggestions = provider.onInputChanged("Example")
            assertEquals(2, suggestions.size)

            assertEquals(tab2.id, suggestions[0].id)
            assertEquals(tab4.id, suggestions[1].id)
            assertEquals("Switch to tab", suggestions[0].description)
            assertEquals("Switch to tab", suggestions[1].description)
        }
    }

    @Test
    fun `Provider returns Sessions with matching titles`() = runTest {
        val tab1 = createTab("https://allizom.org", title = "Internet for people, not profit — Mozilla")
        val tab2 = createTab("https://getpocket.com", title = "Pocket: My List")
        val tab3 = createTab("https://firefox.com", title = "Download Firefox — Free Web Browser")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2, tab3),
            ),
        )

        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val provider = SessionSuggestionProvider(resources, store, mock())

        run {
            val suggestions = provider.onInputChanged("Browser")
            assertEquals(1, suggestions.size)

            assertEquals(suggestions.first().id, tab3.id)
            assertEquals("Switch to tab", suggestions.first().description)
            assertEquals("Download Firefox — Free Web Browser", suggestions[0].title)
        }

        run {
            val suggestions = provider.onInputChanged("Mozilla")
            assertEquals(1, suggestions.size)

            assertEquals(tab1.id, suggestions.first().id)
            assertEquals("Switch to tab", suggestions.first().description)
            assertEquals("Internet for people, not profit — Mozilla", suggestions[0].title)
        }
    }

    @Test
    fun `Provider only returns non-private Sessions`() = runTest {
        val tab = createTab("https://www.mozilla.org")
        val privateTab1 = createTab("https://mozilla.org/firefox", private = true)
        val privateTab2 = createTab("https://mozilla.org/projects", private = true)

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab, privateTab1, privateTab2),
            ),
        )

        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, store, useCase)
        val suggestions = provider.onInputChanged("mozilla")

        assertEquals(1, suggestions.size)
    }

    @Test
    fun `Clicking suggestion invokes SelectTabUseCase`() = runTest {
        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val tab = createTab("https://www.mozilla.org")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
            ),
        )

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, store, useCase)
        val suggestions = provider.onInputChanged("mozilla")
        assertEquals(1, suggestions.size)

        val suggestion = suggestions[0]

        verify(useCase, never()).invoke(tab.id)

        suggestion.onSuggestionClicked!!.invoke()

        verify(useCase).invoke(tab.id)
    }

    @Test
    fun `When excludeSelectedSession is true provider should not include the selected session`() = runTest {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://wikipedia.org"),
                    createTab(id = "b", url = "https://www.mozilla.org"),
                ),
                selectedTabId = "b",
            ),
        )

        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, store, useCase, excludeSelectedSession = true)
        val suggestions = provider.onInputChanged("org")

        assertEquals(1, suggestions.size)
        assertEquals("a", suggestions.first().id)
        assertEquals("Switch to tab", suggestions.first().description)
    }

    @Test
    fun `When excludeSelectedSession is false provider should include the selected session`() = runTest {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://wikipedia.org"),
                    createTab(id = "b", url = "https://www.mozilla.org"),
                ),
                selectedTabId = "b",
            ),
        )

        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, store, useCase, excludeSelectedSession = false)
        val suggestions = provider.onInputChanged("mozilla")

        assertEquals(1, suggestions.size)
        assertEquals("b", suggestions.first().id)
        assertEquals("Switch to tab", suggestions.first().description)
    }

    @Test
    fun `Uses title for chip title when available, but falls back to URL`() = runTest {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://wikipedia.org", title = "Wikipedia"),
                    createTab(id = "b", url = "https://www.mozilla.org", title = ""),
                ),
                selectedTabId = "b",
            ),
        )

        val resources: Resources = mock()
        `when`(resources.getString(anyInt())).thenReturn("Switch to tab")

        val useCase: TabsUseCases.SelectTabUseCase = mock()

        val provider = SessionSuggestionProvider(resources, store, useCase, excludeSelectedSession = false)
        var suggestions = provider.onInputChanged("mozilla")

        assertEquals(1, suggestions.size)
        assertEquals("b", suggestions.first().id)
        assertEquals("https://www.mozilla.org", suggestions.first().title)
        assertEquals("Switch to tab", suggestions.first().description)

        suggestions = provider.onInputChanged("wiki")
        assertEquals(1, suggestions.size)
        assertEquals("a", suggestions.first().id)
        assertEquals("Wikipedia", suggestions.first().title)
        assertEquals("Switch to tab", suggestions.first().description)
    }
}
