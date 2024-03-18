/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController
import org.mozilla.fenix.settings.logins.interactor.AddLoginInteractor

class AddLoginInteractorTest {

    private val loginsController: SavedLoginsStorageController = mockk(relaxed = true)
    private val interactor = AddLoginInteractor(loginsController)

    private val hostname = "https://www.cats.com"
    private val username = "myFunUsername111"
    private val password = "superDuperSecure123!"

    @Test
    fun findPotentialDupesTest() {
        interactor.findDuplicate(
            hostname,
            username,
            password,
        )

        verify {
            loginsController.findDuplicateForAdd(
                hostname,
                username,
                password,
            )
        }
    }

    @Test
    fun addNewLoginTest() {
        interactor.onAddLogin(hostname, username, password)

        verify {
            loginsController.add(
                hostname,
                username,
                password,
            )
        }
    }
}
