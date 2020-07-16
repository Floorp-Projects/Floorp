/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyAll
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SavedLoginsControllerTest {
    private val store: LoginsFragmentStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val browserNavigator: (String, Boolean, BrowserDirection) -> Unit = mockk(relaxed = true)

    private val settings: Settings = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val sortingStrategy: SortingStrategy = SortingStrategy.Alphabetically(testContext)
    private val controller = SavedLoginsController(store, navController, browserNavigator, settings, metrics)

    @Test
    fun `GIVEN a sorting strategy, WHEN handleSort is called on the controller, THEN the correct action should be dispatched and the strategy saved in sharedPref`() {
        controller.handleSort(sortingStrategy)

        verify {
            store.dispatch(
                LoginsAction.SortLogins(
                    SortingStrategy.Alphabetically(
                        testContext
                    )
                )
            )
            settings.savedLoginsSortingStrategy = sortingStrategy
        }
    }

    @Test
    fun `GIVEN a SavedLogin, WHEN handleItemClicked is called for it, THEN LoginsAction$LoginSelected should be emitted`() {
        val login: SavedLogin = mockk(relaxed = true)

        controller.handleItemClicked(login)

        verifyAll {
            store.dispatch(LoginsAction.LoginSelected(login))
            metrics.track(Event.OpenOneLogin)
            navController.navigate(
                    SavedLoginsFragmentDirections.actionSavedLoginsFragmentToLoginDetailFragment(login.guid)
            )
        }
    }

    @Test
    fun `GIVEN the learn more option, WHEN handleLearnMoreClicked is called for it, then we should open the right support webpage`() {
        controller.handleLearnMoreClicked()

        verify {
            browserNavigator.invoke(
                SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.SYNC_SETUP),
                true,
                BrowserDirection.FromSavedLoginsFragment
            )
        }
    }
}
