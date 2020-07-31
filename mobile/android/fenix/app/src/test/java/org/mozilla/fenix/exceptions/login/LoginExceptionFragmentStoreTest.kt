/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.login

import io.mockk.mockk
import mozilla.components.feature.logins.exceptions.LoginException
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class LoginExceptionFragmentStoreTest {

    @Test
    fun onChange() {
        val initialState = ExceptionsFragmentState()
        val store = ExceptionsFragmentStore(initialState)
        val newExceptionsItem: LoginException = mockk()

        store.dispatch(ExceptionsFragmentAction.Change(listOf(newExceptionsItem))).joinBlocking()
        assertNotSame(initialState, store.state)
        assertEquals(listOf(newExceptionsItem), store.state.items)
    }
}
