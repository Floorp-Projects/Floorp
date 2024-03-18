/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.recover.TabState
import org.junit.Before
import org.junit.Test

class RecentlyClosedFragmentInteractorTest {

    lateinit var interactor: RecentlyClosedFragmentInteractor
    private val defaultRecentlyClosedController: DefaultRecentlyClosedController =
        mockk(relaxed = true)

    @Before
    fun setup() {
        interactor =
            RecentlyClosedFragmentInteractor(
                recentlyClosedController = defaultRecentlyClosedController,
            )
    }

    @Test
    fun onDelete() {
        val tab = TabState(id = "tab-id", title = "Mozilla", url = "mozilla.org", lastAccess = 1L)
        interactor.onDelete(tab)

        verify {
            defaultRecentlyClosedController.handleDelete(tab)
        }
    }

    @Test
    fun onNavigateToHistory() {
        interactor.onNavigateToHistory()

        verify {
            defaultRecentlyClosedController.handleNavigateToHistory()
        }
    }
}
