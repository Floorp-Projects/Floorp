package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.service.fxa.ServerConfig
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.isInExperiment

object FxaServer {
    const val CLIENT_ID = "a2270f727f45f648"
    const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"

    fun redirectUrl(context: Context) = if (context.isInExperiment(Experiments.asFeatureWebChannelsDisabled)) {
        REDIRECT_URL
    } else {
        "urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel"
    }

    fun config(context: Context): ServerConfig {
        return ServerConfig.release(CLIENT_ID, redirectUrl(context))
    }
}
