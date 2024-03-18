/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.utils.Settings

class DefaultSearchSelectorControllerTest {

    private val activity: HomeActivity = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)

    private lateinit var controller: DefaultSearchSelectorController

    @Before
    fun setup() {
        controller = DefaultSearchSelectorController(
            activity = activity,
            navController = navController,
        )

        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }
        every { activity.components.settings } returns settings
        every { activity.settings() } returns settings
    }

    @Test
    fun `WHEN the search settings menu item is tapped THEN navigate to search engine settings fragment`() {
        controller.handleMenuItemTapped(SearchSelectorMenu.Item.SearchSettings)

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_searchEngineFragment },
                null,
            )
        }
    }

    @Test
    fun `WHEN a search engine menu item is tapped THEN open the search dialog with the selected search engine`() {
        val item = mockk<SearchSelectorMenu.Item.SearchEngine>()
        every { item.searchEngine.id } returns "DuckDuckGo"

        controller.handleMenuItemTapped(item)

        val expectedDirections = HomeFragmentDirections.actionGlobalSearchDialog(
            sessionId = null,
            searchEngine = item.searchEngine.id,
        )
        verify {
            navController.navigate(expectedDirections, any<NavOptions>())
        }
    }
}
