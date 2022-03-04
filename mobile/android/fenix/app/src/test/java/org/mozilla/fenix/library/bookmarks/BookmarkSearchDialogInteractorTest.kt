/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runBlockingTest
import org.junit.Before
import org.junit.Test

class BookmarkSearchDialogInteractorTest {

    lateinit var searchController: BookmarkSearchDialogController
    lateinit var interactor: BookmarkSearchDialogInteractor

    @Before
    fun setup() {
        searchController = mockk(relaxed = true)
        interactor = BookmarkSearchDialogInteractor(
            searchController
        )
    }

    @Test
    fun onEditingCanceled() = runBlockingTest {
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
