/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class ExceptionsStoreTest {
    @Test
    fun onChange() = runBlocking {
        val initialState = emptyDefaultState()
        val store = ExceptionsStore(initialState)
        val newExceptionsItem = ExceptionsItem("URL")

        store.dispatch(ExceptionsAction.Change(listOf(newExceptionsItem))).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.items,
            listOf(newExceptionsItem)
        )
    }

    private fun emptyDefaultState(): ExceptionsState = ExceptionsState(
        items = listOf()
    )
}
