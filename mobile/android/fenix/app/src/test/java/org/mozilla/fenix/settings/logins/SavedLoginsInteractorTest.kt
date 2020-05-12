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
import kotlin.random.Random

@RunWith(FenixRobolectricTestRunner::class)
class SavedLoginsInteractorTest {
    private val controller: SavedLoginsController = mockk(relaxed = true)
    private val savedLoginClicked: (SavedLogin) -> Unit = mockk(relaxed = true)
    private val learnMore: () -> Unit = mockk(relaxed = true)
    private val interactor = SavedLoginsInteractor(
        controller,
        savedLoginClicked,
        learnMore
    )

    @Test
    fun itemClicked() {
        val item = SavedLogin("mozilla.org", "username", "password", "id", Random.nextLong())
        interactor.itemClicked(item)

        verify {
            savedLoginClicked.invoke(item)
        }
    }

    @Test
    fun `GIVEN a sorting strategy, WHEN sort method is called on the interactor, THEN controller should call handleSort with the same parameter`() {
        val sortingStrategy: SortingStrategy = SortingStrategy.Alphabetically(testContext)

        interactor.sort(sortingStrategy)

        verify {
            controller.handleSort(sortingStrategy)
        }
    }
}
