/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import org.junit.Before
import org.junit.Test

class HistorySearchDialogInteractorTest {

    lateinit var searchController: HistorySearchDialogController
    lateinit var interactor: HistorySearchDialogInteractor

    @Before
    fun setup() {
        searchController = mockk(relaxed = true)
        interactor = HistorySearchDialogInteractor(
            searchController
        )
    }

    @Test
    fun onEditingCanceled() = runTest {
        interactor.onEditingCanceled()

        verify {
            searchController.handleEditingCancelled()
        }
    }

    @Test
    fun onTextChanged() {
        interactor.onTextChanged("test")

        verify { searchController.handleTextChanged("test") }
    }

    @Test
    fun onUrlTapped() {
        interactor.onUrlTapped("test")

        verify {
            searchController.handleUrlTapped("test")
        }
    }
}
