/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.interactor

import org.mozilla.fenix.settings.logins.controller.SavedLoginsStorageController

/**
 * Interactor for the add login screen
 *
 * @param savedLoginsController controller for the saved logins storage
 */
class AddLoginInteractor(
    private val savedLoginsController: SavedLoginsStorageController,
) {
    fun findDuplicate(originText: String, usernameText: String, passwordText: String) {
        savedLoginsController.findDuplicateForAdd(originText, usernameText, passwordText)
    }

    fun onAddLogin(originText: String, usernameText: String, passwordText: String) {
        savedLoginsController.add(originText, usernameText, passwordText)
    }
}
