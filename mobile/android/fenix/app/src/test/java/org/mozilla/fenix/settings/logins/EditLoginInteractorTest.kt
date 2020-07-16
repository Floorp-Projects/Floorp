/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.interactor.EditLoginInteractor

class EditLoginInteractorTest {
    private val loginsController: SavedLoginsStorageController = mockk(relaxed = true)
    private val interactor = EditLoginInteractor(loginsController)

    @Test
    fun findPotentialDupesTest() {
        val id = "anyId"
        interactor.findPotentialDuplicates(id)
        verify { loginsController.findPotentialDuplicates(id) }
    }

    @Test
    fun saveLoginTest() {
        val id = "anyId"
        val username = "usernameText"
        val password = "passwordText"

        interactor.onSaveLogin(id, username, password)

        verify { loginsController.save(id, username, password) }
    }
}
