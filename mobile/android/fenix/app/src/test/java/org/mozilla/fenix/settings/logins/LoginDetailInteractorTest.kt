/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.mockk
import io.mockk.verifyAll
import org.junit.Test
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.interactor.LoginDetailInteractor

class LoginDetailInteractorTest {
    private val loginsController: SavedLoginsStorageController = mockk(relaxed = true)
    private val interactor = LoginDetailInteractor(loginsController)

    @Test
    fun fetchLoginListTest() {
        val id = "anyId"
        interactor.onFetchLoginList(id)
        verifyAll { loginsController.fetchLoginDetails(id) }
    }

    @Test
    fun deleteLoginTest() {
        val id = "anyId"
        interactor.onDeleteLogin(id)
        verifyAll { loginsController.delete(id) }
    }
}
