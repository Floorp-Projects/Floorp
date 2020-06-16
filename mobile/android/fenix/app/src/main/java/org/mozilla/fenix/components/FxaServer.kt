/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.ServerConfig.Server
import org.mozilla.fenix.ext.settings

/**
 * Utility to configure Firefox Account servers.
 */

object FxaServer {
    private const val CLIENT_ID = "a2270f727f45f648"

    const val REDIRECT_URL = "urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel"

    fun config(context: Context): ServerConfig {
        val serverOverride = context.settings().overrideFxAServer
        val tokenServerOverride = context.settings().overrideSyncTokenServer.ifEmpty { null }
        if (serverOverride.isEmpty()) {
            return ServerConfig(Server.RELEASE, CLIENT_ID, redirectUrl(), tokenServerOverride)
        }
        return ServerConfig(serverOverride, CLIENT_ID, redirectUrl(), tokenServerOverride)
    }
}
