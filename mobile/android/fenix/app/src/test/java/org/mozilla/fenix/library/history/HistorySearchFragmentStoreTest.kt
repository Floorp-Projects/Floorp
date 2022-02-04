/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import io.mockk.impl.annotations.MockK
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test
import org.mozilla.fenix.components.Components

class HistorySearchFragmentStoreTest {

    @MockK(relaxed = true) private lateinit var components: Components

    @Test
    fun `GIVEN createInitialHistorySearchFragmentState THEN query is empty`() {
        val expected = HistorySearchFragmentState(query = "")

        assertEquals(
            expected,
            createInitialHistorySearchFragmentState()
        )
    }

    @Test
    fun updateQuery() = runBlocking {
        val initialState = HistorySearchFragmentState(query = "")
        val store = HistorySearchFragmentStore(initialState)
        val query = "test query"

        store.dispatch(HistorySearchFragmentAction.UpdateQuery(query)).join()
        assertNotSame(initialState, store.state)
        assertEquals(query, store.state.query)
    }
}
