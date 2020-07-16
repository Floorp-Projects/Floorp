/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.mockk
import io.mockk.verifyAll
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import kotlin.random.Random

@RunWith(FenixRobolectricTestRunner::class)
class SavedLoginsInteractorTest {
    private val controller: SavedLoginsController = mockk(relaxed = true)
    private val interactor = SavedLoginsInteractor(controller)

    @Test
    fun `GIVEN a SavedLogin being clicked, WHEN the interactor is called for it, THEN it should just delegate the controller`() {
        val item = SavedLogin("mozilla.org", "username", "password", "id", Random.nextLong())
        interactor.onItemClicked(item)

        verifyAll {
            controller.handleItemClicked(item)
        }
    }

    @Test
    fun `GIVEN a change in sorting strategy, WHEN the interactor is called for it, THEN it should just delegate the controller`() {
        val sortingStrategy: SortingStrategy = SortingStrategy.Alphabetically(testContext)

        interactor.onSortingStrategyChanged(sortingStrategy)

        verifyAll {
            controller.handleSort(sortingStrategy)
        }
    }

    @Test
    fun `GIVEN the learn more option is clicked, WHEN the interactor is called for it, THEN it should just delegate the controller`() {
        interactor.onLearnMoreClicked()

        verifyAll {
            controller.handleLearnMoreClicked()
        }
    }
}
