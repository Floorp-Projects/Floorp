/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FxaAccountManager

/**
 * Component group for background services. These are the components that need to be accessed from within a
 * background worker.
 */
class BackgroundServices(
    context: Context
) {
    companion object {
        const val CLIENT_ID = "a2270f727f45f648"
        const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"
        const val SUCCESS_PATH = "connect_another_device?showSuccessMessage=true"
    }

    // This is slightly messy - here we need to know the union of all "scopes"
    // needed by components which rely on FxA integration. If this list
    // grows too far we probably want to find a way to determine the set
    // at runtime.
    private val scopes: Array<String> = arrayOf("profile", "https://identity.mozilla.com/apps/oldsync")
    private val config = Config.release(CLIENT_ID, REDIRECT_URL)

    val accountManager = FxaAccountManager(context, config, scopes).also { it.initAsync() }
}
