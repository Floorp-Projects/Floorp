/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.login

import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.feature.logins.exceptions.LoginException
import mozilla.components.feature.logins.exceptions.LoginExceptionStorage
import org.junit.Before
import org.junit.Test

@ExperimentalCoroutinesApi
class LoginExceptionsInteractorTest {

    private lateinit var loginExceptionStorage: LoginExceptionStorage
    private lateinit var interactor: LoginExceptionsInteractor
    private val scope = TestCoroutineScope()

    @Before
    fun setup() {
        loginExceptionStorage = mockk(relaxed = true)
        interactor = DefaultLoginExceptionsInteractor(scope, loginExceptionStorage)
    }

    @Test
    fun onDeleteAll() = scope.runBlockingTest {
        interactor.onDeleteAll()
        verify { loginExceptionStorage.deleteAllLoginExceptions() }
    }

    @Test
    fun onDeleteOne() = scope.runBlockingTest {
        val exceptionsItem: LoginException = mockk()
        interactor.onDeleteOne(exceptionsItem)
        verify { loginExceptionStorage.removeLoginException(exceptionsItem) }
    }
}
