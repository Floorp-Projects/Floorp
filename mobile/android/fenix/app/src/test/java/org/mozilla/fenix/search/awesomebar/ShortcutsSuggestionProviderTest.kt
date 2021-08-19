/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import android.content.Context
import androidx.appcompat.content.res.AppCompatResources
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.store.BrowserStore
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R

@ExperimentalCoroutinesApi
class ShortcutsSuggestionProviderTest {

    private lateinit var context: Context

    @Before
    fun setup() {
        mockkStatic(AppCompatResources::class)
        context = mockk {
            every { getString(R.string.search_shortcuts_engine_settings) } returns "Search engine settings"
        }

        every { AppCompatResources.getDrawable(context, R.drawable.mozac_ic_settings) } returns null
    }

    @After
    fun teardown() {
        unmockkStatic(AppCompatResources::class)
    }

    @Test
    fun `returns suggestions from search engine provider`() = runBlockingTest {
        val engineOne = mockk<SearchEngine> {
            every { id } returns "1"
            every { name } returns "EngineOne"
            every { icon } returns mockk()
        }
        val engineTwo = mockk<SearchEngine> {
            every { id } returns "2"
            every { name } returns "EngineTwo"
            every { icon } returns mockk()
        }
        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(engineOne, engineTwo)
                )
            )
        )
        val provider = ShortcutsSuggestionProvider(store, context, {}, {})

        val suggestions = provider.onInputChanged("")

        assertEquals(3, suggestions.size)

        assertEquals(provider, suggestions[0].provider)
        assertEquals(engineOne.id, suggestions[0].id)
        assertEquals(engineOne.icon, suggestions[0].icon)
        assertEquals(engineOne.name, suggestions[0].title)

        assertEquals(provider, suggestions[1].provider)
        assertEquals(engineTwo.id, suggestions[1].id)
        assertEquals(engineTwo.icon, suggestions[1].icon)
        assertEquals(engineTwo.name, suggestions[1].title)

        assertEquals(provider, suggestions[2].provider)
        assertEquals("Search engine settings", suggestions[2].id)
        assertEquals("Search engine settings", suggestions[2].title)
    }

    @Test
    fun `callbacks are triggered when suggestions are clicked`() = runBlockingTest {
        val engineOne = mockk<SearchEngine>(relaxed = true)
        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(engineOne)
                )
            )
        )

        var selectEngine: SearchEngine? = null
        var selectShortcutEngineSettingsChanged = false
        val provider = ShortcutsSuggestionProvider(
            store,
            context,
            { selectEngine = it },
            { selectShortcutEngineSettingsChanged = true }
        )

        val suggestions = provider.onInputChanged("")
        assertEquals(2, suggestions.size)

        suggestions[0].onSuggestionClicked?.invoke()
        suggestions[1].onSuggestionClicked?.invoke()

        assertEquals(engineOne, selectEngine)
        assertTrue(selectShortcutEngineSettingsChanged)
    }
}
