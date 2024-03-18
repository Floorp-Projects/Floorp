/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.appservices.fxaclient.FxaConfig
import mozilla.appservices.fxaclient.FxaServer

/**
 * Utility to configure Firefox Account stage servers.
 */

object FxaServer {
    private const val CLIENT_ID = "a2270f727f45f648"
    private const val REDIRECT_URL = "urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel"

    @Suppress("UNUSED_PARAMETER")
    fun config(context: Context): FxaConfig {
        return FxaConfig(FxaServer.Stage, CLIENT_ID, REDIRECT_URL)
    }
}
