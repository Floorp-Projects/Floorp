/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webcompat.reporter

import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject

/**
 * A feature that enables users to report site issues to Mozilla's Web Compatibility team for
 * further diagnosis.
 */
object WebCompatReporterFeature {
    private val logger = Logger("mozac-webcompat-reporter")

    internal const val WEBCOMPAT_REPORTER_EXTENSION_ID = "webcompat-reporter@mozilla.org"
    internal const val WEBCOMPAT_REPORTER_EXTENSION_URL = "resource://android/assets/extensions/webcompat-reporter/"
    internal const val WEBCOMPAT_REPORTER_MESSAGING_ID = "mozacWebcompatReporter"

    private class WebcompatReporterBackgroundMessageHandler(
        // This information will be provided as a browser-XXX label to the reporting backend, allowing
        // us to differentiate different android-components based products.
        private val productName: String
    ) : MessageHandler {
        override fun onPortConnected(port: Port) {
            port.postMessage(JSONObject().put("productName", productName))
        }
    }

    /**
     * Installs the web extension in the runtime through the WebExtensionRuntime install method
     *
     * @param runtime a WebExtensionRuntime.
     * @param productName a custom product name used to automatically label reports. Defaults to
     * "android-components".
     */
    fun install(runtime: WebExtensionRuntime, productName: String = "android-components") {
        runtime.installWebExtension(WEBCOMPAT_REPORTER_EXTENSION_ID, WEBCOMPAT_REPORTER_EXTENSION_URL,
            onSuccess = {
                logger.debug("Installed WebCompat Reporter webextension: ${it.id}")
                it.registerBackgroundMessageHandler(
                    WEBCOMPAT_REPORTER_MESSAGING_ID,
                    WebcompatReporterBackgroundMessageHandler(productName)
                )
            },
            onError = { ext, throwable ->
                logger.error("Failed to install WebCompat Reporter webextension: $ext", throwable)
            }
        )
    }
}
