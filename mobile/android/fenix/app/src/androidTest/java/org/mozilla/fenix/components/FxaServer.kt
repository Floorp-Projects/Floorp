/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.service.fxa.ServerConfig.Server
import mozilla.components.service.fxa.ServerConfig

/**
 * Utility to configure Firefox Account stage servers.
 */

object FxaServer {
    const val CLIENT_ID = "a2270f727f45f648"
    const val REDIRECT_URL = "https://accounts.stage.mozaws.net/oauth/success/$CLIENT_ID"

    @Suppress("UNUSED_PARAMETER")
    fun redirectUrl(context: Context) = REDIRECT_URL

    @Suppress("UNUSED_PARAMETER")
    fun config(context: Context): ServerConfig {
        return ServerConfig(Server.STAGE, CLIENT_ID, REDIRECT_URL)
    }
}
