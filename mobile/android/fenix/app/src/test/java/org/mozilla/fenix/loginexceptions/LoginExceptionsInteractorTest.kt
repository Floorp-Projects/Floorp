/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions

import io.mockk.mockk
import mozilla.components.feature.logins.exceptions.LoginException
import org.junit.Assert.assertEquals
import org.junit.Test

class LoginExceptionsInteractorTest {

    @Test
    fun onDeleteAll() {
        var onDeleteAll = false
        val interactor = LoginExceptionsInteractor(
            mockk(),
            { onDeleteAll = true }
        )
        interactor.onDeleteAll()
        assertEquals(true, onDeleteAll)
    }

    @Test
    fun onDeleteOne() {
        var exceptionsItemReceived: LoginException? = null
        val exceptionsItem: LoginException = mockk()
        val interactor = LoginExceptionsInteractor(
            { exceptionsItemReceived = exceptionsItem },
            mockk()
        )
        interactor.onDeleteOne(exceptionsItem)
        assertEquals(exceptionsItemReceived, exceptionsItem)
    }
}
