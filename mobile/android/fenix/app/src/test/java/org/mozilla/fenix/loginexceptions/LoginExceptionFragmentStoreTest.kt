/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions

import io.mockk.mockk
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.logins.exceptions.LoginException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class LoginExceptionFragmentStoreTest {
    @Test
    fun onChange() = runBlocking {
        val initialState = emptyDefaultState()
        val store = ExceptionsFragmentStore(initialState)
        val newExceptionsItem: LoginException = mockk()

        store.dispatch(ExceptionsFragmentAction.Change(listOf(newExceptionsItem))).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.items,
            listOf(newExceptionsItem)
        )
    }

    private fun emptyDefaultState(): ExceptionsFragmentState = ExceptionsFragmentState(
        items = listOf()
    )
}
