/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webcompat.reporter

import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.support.base.log.logger.Logger

/**
 * Feature to enable website-hotfixing via the Web Compatibility System-Addon.
 */
object WebCompatReporterFeature {
    private val logger = Logger("mozac-webcompat-reporter")

    internal const val WEBCOMPAT_REPORTER_EXTENSION_ID = "webcompat-reporter@mozilla.org"
    internal const val WEBCOMPAT_REPORTER_EXTENSION_URL = "resource://android/assets/extensions/webcompat-reporter/"

    /**
     * Installs the web extension in the runtime through the WebExtensionRuntime install method
     */
    fun install(runtime: WebExtensionRuntime) {
        runtime.installWebExtension(WEBCOMPAT_REPORTER_EXTENSION_ID, WEBCOMPAT_REPORTER_EXTENSION_URL,
            supportActions = true,
            onSuccess = {
                logger.debug("Installed WebCompat Reporter webextension: ${it.id}")
            },
            onError = { ext, throwable ->
                logger.error("Failed to install WebCompat Reporter webextension: $ext", throwable)
            }
        )
    }
}
