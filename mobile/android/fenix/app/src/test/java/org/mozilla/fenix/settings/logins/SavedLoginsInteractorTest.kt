/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.every
import io.mockk.mockk
import io.mockk.verifyAll
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.logins.controller.LoginsListController
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor
import kotlin.random.Random

@RunWith(FenixRobolectricTestRunner::class)
class SavedLoginsInteractorTest {
    private val listController: LoginsListController = mockk(relaxed = true)
    private val savedLoginsStorageController: SavedLoginsStorageController = mockk(relaxed = true)
    private lateinit var interactor: SavedLoginsInteractor

    @Before
    fun setup() {
        interactor = SavedLoginsInteractor(listController, savedLoginsStorageController)
    }

    @Test
    fun `GIVEN a SavedLogin being clicked, WHEN the interactor is called for it, THEN it should just delegate the controller`() {
        val item = SavedLogin("mozilla.org", "username", "password", "id", Random.nextLong())
        interactor.onItemClicked(item)

        verifyAll {
            listController.handleItemClicked(item)
        }
    }

    @Test
    fun `GIVEN a change in sorting strategy, WHEN the interactor is called for it, THEN it should just delegate the controller`() {
        every { testContext.components.publicSuffixList } returns PublicSuffixList(testContext)
        interactor.onSortingStrategyChanged(SortingStrategy.Alphabetically)

        verifyAll {
            listController.handleSort(SortingStrategy.Alphabetically)
        }
    }

    @Test
    fun `GIVEN the learn more option is clicked, WHEN the interactor is called for it, THEN it should just delegate the controller`() {
        interactor.onLearnMoreClicked()

        verifyAll {
            listController.handleLearnMoreClicked()
        }
    }

    @Test
    fun loadAndMapLoginsTest() {
        interactor.loadAndMapLogins()
        verifyAll { savedLoginsStorageController.handleLoadAndMapLogins() }
    }

    @Test
    fun `Handle add login button click`() {
        interactor.onAddLoginClick()
        verifyAll { listController.handleAddLoginClicked() }
    }
}
