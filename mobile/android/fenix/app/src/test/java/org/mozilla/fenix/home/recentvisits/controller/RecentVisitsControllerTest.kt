/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.controller

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.feature.tabs.TabsUseCases.SelectOrAddUseCase
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
class RecentVisitsControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val selectOrAddTabUseCase: SelectOrAddUseCase = mockk(relaxed = true)
    private val navController = mockk<NavController>(relaxed = true)

    private lateinit var storage: HistoryMetadataStorage
    private lateinit var appStore: AppStore
    private lateinit var store: BrowserStore

    private lateinit var controller: DefaultRecentVisitsController

    @Before
    fun setup() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }
        storage = mockk(relaxed = true)
        appStore = mockk(relaxed = true)
        store = mockk(relaxed = true)

        controller = spyk(
            DefaultRecentVisitsController(
                appStore = appStore,
                store = store,
                selectOrAddTabUseCase = selectOrAddTabUseCase,
                navController = navController,
                scope = scope,
                storage = storage,
            )
        )
    }

    @Test
    fun handleHistoryShowAllClicked() = runTestOnMain {
        controller.handleHistoryShowAllClicked()

        verify {
            controller.dismissSearchDialogIfDisplayed()
            navController.navigate(
                HomeFragmentDirections.actionGlobalHistoryFragment()
            )
        }
    }

    @Test
    fun handleRecentHistoryHighlightClicked() = runTestOnMain {
        val historyHighlight = RecentHistoryHighlight("title", "url")

        controller.handleRecentHistoryHighlightClicked(historyHighlight)

        verifyOrder {
            selectOrAddTabUseCase.invoke(historyHighlight.url)
            navController.navigate(R.id.browserFragment)
        }
    }

    @Test
    fun handleRemoveRecentHistoryHighlight() = runTestOnMain {
        val highlightUrl = "highlightUrl"
        controller.handleRemoveRecentHistoryHighlight(highlightUrl)

        verify {
            appStore.dispatch(AppAction.RemoveRecentHistoryHighlight(highlightUrl))
            scope.launch {
                storage.deleteHistoryMetadataForUrl(highlightUrl)
            }
        }
    }
}
