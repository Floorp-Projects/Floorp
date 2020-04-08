/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SavedLoginsControllerTest {
    private val store: SavedLoginsFragmentStore = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val sortingStrategy: SortingStrategy = SortingStrategy.Alphabetically(testContext)
    private val controller = DefaultSavedLoginsController(store, settings)

    @Test
    fun `GIVEN a sorting strategy, WHEN handleSort is called on the controller, THEN the correct action should be dispatched and the strategy saved in sharedPref`() {
        controller.handleSort(sortingStrategy)

        verify {
            store.dispatch(
                SavedLoginsFragmentAction.SortLogins(
                    SortingStrategy.Alphabetically(
                        testContext
                    )
                )
            )
            settings.savedLoginsSortingStrategy = sortingStrategy
        }
    }
}
