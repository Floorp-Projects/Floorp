/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.fragment.app.Fragment
import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import io.mockk.Called
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import org.junit.After
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.ext.requireComponents

internal class SearchDialogFragmentTest {
    private val navController: NavController = mockk()
    private val fragment = SearchDialogFragment()

    @Before
    fun setup() {
        mockkStatic("androidx.navigation.fragment.FragmentKt")
        every { any<Fragment>().findNavController() } returns navController
    }

    @After
    fun teardown() {
        unmockkStatic("androidx.navigation.fragment.FragmentKt")
    }

    @Test
    fun `GIVEN this is the only visible fragment WHEN asking for the previous destination THEN return null`() {
        every { navController.backQueue } returns ArrayDeque(listOf(getDestination(fragmentName)))

        assertNull(fragment.getPreviousDestination())
    }

    @Test
    fun `GIVEN this and FragmentB on top of this are visible WHEN asking for the previous destination THEN return null`() {
        every { navController.backQueue } returns ArrayDeque(
            listOf(
                getDestination(fragmentName),
                getDestination("FragmentB"),
            ),
        )

        assertNull(fragment.getPreviousDestination())
    }

    @Test
    fun `GIVEN FragmentA, this and FragmentB are visible WHEN asking for the previous destination THEN return FragmentA`() {
        val fragmentADestination = getDestination("FragmentA")
        every { navController.backQueue } returns ArrayDeque(
            listOf(
                fragmentADestination,
                getDestination(fragmentName),
                getDestination("FragmentB"),
            ),
        )

        assertSame(fragmentADestination, fragment.getPreviousDestination())
    }

    @Test
    fun `GIVEN FragmentA and this on top of it are visible WHEN asking for the previous destination THEN return FragmentA`() {
        val fragmentADestination = getDestination("FragmentA")
        every { navController.backQueue } returns ArrayDeque(
            listOf(
                fragmentADestination,
                getDestination(fragmentName),
            ),
        )

        assertSame(fragmentADestination, fragment.getPreviousDestination())
    }

    @Test
    fun `GIVEN the default search engine is currently selected WHEN checking the need to update the current search engine THEN don't to anything`() {
        fragment.interactor = mockk()
        val defaultSearchEngine: SearchEngine = mockk {
            every { id } returns "default"
        }
        val otherSearchEngine: SearchEngine = mockk {
            every { id } returns "other"
        }
        val components: Components = mockk {
            every { core.store.state.search } returns mockk(relaxed = true) {
                every { searchEngines } returns listOf(otherSearchEngine, defaultSearchEngine)
            }
        }
        mockkStatic(
            "org.mozilla.fenix.ext.FragmentKt",
            "mozilla.components.browser.state.state.SearchStateKt",
        ) {
            every { any<Fragment>().requireComponents } returns components
            every { any<SearchState>().selectedOrDefaultSearchEngine } returns defaultSearchEngine

            fragment.maybeSelectShortcutEngine(defaultSearchEngine.id)

            verify { fragment.interactor wasNot Called }
        }
    }

    @Test
    fun `GIVEN the default search engine is not currently selected WHEN checking the need to update the current search engine THEN update it to the current engine`() {
        fragment.interactor = mockk {
            every { onSearchShortcutEngineSelected(any()) } just Runs
        }
        val defaultSearchEngine: SearchEngine = mockk {
            every { id } returns "default"
        }
        val otherSearchEngine: SearchEngine = mockk {
            every { id } returns "other"
        }
        val components: Components = mockk {
            every { core.store.state.search } returns mockk(relaxed = true) {
                every { searchEngines } returns listOf(otherSearchEngine, defaultSearchEngine)
            }
        }
        mockkStatic(
            "org.mozilla.fenix.ext.FragmentKt",
            "mozilla.components.browser.state.state.SearchStateKt",
        ) {
            every { any<Fragment>().requireComponents } returns components
            every { any<SearchState>().selectedOrDefaultSearchEngine } returns defaultSearchEngine

            fragment.maybeSelectShortcutEngine(otherSearchEngine.id)

            verify { fragment.interactor.onSearchShortcutEngineSelected(otherSearchEngine) }
        }
    }

    @Test
    fun `GIVEN the currently selected search engine is unknown WHEN checking the need to update the current search engine THEN don't do anything`() {
        fragment.interactor = mockk()

        fragment.maybeSelectShortcutEngine(null)

        verify { fragment.interactor wasNot Called }
    }
}

private val fragmentName = SearchDialogFragment::class.java.canonicalName?.substringAfterLast('.')!!

private fun getDestination(destinationName: String): NavBackStackEntry {
    return mockk {
        every { destination } returns mockk {
            every { displayName } returns "test.id/$destinationName"
        }
    }
}
