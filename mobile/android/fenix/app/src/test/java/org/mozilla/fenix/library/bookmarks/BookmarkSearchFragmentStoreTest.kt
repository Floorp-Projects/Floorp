/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class BookmarkSearchFragmentStoreTest {

    @Test
    fun `GIVEN createInitialBookmarkSearchFragmentState THEN query is empty`() {
        val expected = BookmarkSearchFragmentState(query = "")

        assertEquals(
            expected,
            createInitialBookmarkSearchFragmentState()
        )
    }

    @Test
    fun updateQuery() = runTest {
        val initialState = BookmarkSearchFragmentState(query = "")
        val store = BookmarkSearchFragmentStore(initialState)
        val query = "test query"

        store.dispatch(BookmarkSearchFragmentAction.UpdateQuery(query)).join()
        assertNotSame(initialState, store.state)
        assertEquals(query, store.state.query)
    }
}
