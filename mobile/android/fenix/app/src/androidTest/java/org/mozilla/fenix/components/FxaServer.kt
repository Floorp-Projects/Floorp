package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.service.fxa.ServerConfig

object FxaServer {
    const val CLIENT_ID = "a2270f727f45f648"
    const val REDIRECT_URL = "https://accounts.stage.mozaws.net/oauth/success/$CLIENT_ID"

    @Suppress("UNUSED_PARAMETER")
    fun redirectUrl(context: Context) = REDIRECT_URL

    @Suppress("UNUSED_PARAMETER")
    fun config(context: Context): ServerConfig {
        return ServerConfig.dev(CLIENT_ID, REDIRECT_URL)
    }
}
