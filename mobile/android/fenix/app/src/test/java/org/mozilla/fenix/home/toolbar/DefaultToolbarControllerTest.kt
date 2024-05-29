/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.toolbar

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class) // For gleanTestRule
class DefaultToolbarControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)

    private val searchEngine = SearchEngine(
        id = "test",
        name = "Test Engine",
        icon = mockk(relaxed = true),
        type = SearchEngine.Type.BUNDLED,
        resultUrls = listOf("https://example.org/?q={searchTerms}"),
    )

    private lateinit var store: BrowserStore

    @Before
    fun setup() {
        store = BrowserStore(
            initialState = BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(searchEngine),
                ),
            ),
        )

        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }
        every { activity.settings() } returns settings
    }

    @Test
    fun `WHEN Paste & Go toolbar menu is clicked THEN open the browser with the clipboard text as the search term`() {
        assertNull(Events.enteredUrl.testGetValue())
        assertNull(Events.performedSearch.testGetValue())

        var clipboardText = "text"
        createController().handlePasteAndGo(clipboardText)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = clipboardText,
                newTab = true,
                from = BrowserDirection.FromHome,
                engine = searchEngine,
            )
        }

        assertNotNull(Events.performedSearch.testGetValue())

        clipboardText = "https://mozilla.org"
        createController().handlePasteAndGo(clipboardText)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = clipboardText,
                newTab = true,
                from = BrowserDirection.FromHome,
                engine = searchEngine,
            )
        }

        assertNotNull(Events.enteredUrl.testGetValue())
    }

    @Test
    fun `WHEN Paste toolbar menu is clicked THEN navigate to the search dialog`() {
        createController().handlePaste("text")

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_search_dialog },
                null,
            )
        }
    }

    @Test
    fun `WHEN the toolbar is tapped THEN navigate to the search dialog`() {
        assertNull(Events.searchBarTapped.testGetValue())

        createController().handleNavigateSearch()

        assertNotNull(Events.searchBarTapped.testGetValue())

        val recordedEvents = Events.searchBarTapped.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals("HOME", recordedEvents.single().extra?.getValue("source"))

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_search_dialog },
                any<NavOptions>(),
            )
        }
    }

    private fun createController() = DefaultToolbarController(
        activity = activity,
        store = store,
        navController = navController,
    )
}
