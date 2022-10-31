/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webcompat

import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.support.base.log.logger.Logger

/**
 * Feature to enable website-hotfixing via the Web Compatibility System-Addon.
 */
object WebCompatFeature {
    private val logger = Logger("mozac-webcompat")

    internal const val WEBCOMPAT_EXTENSION_ID = "webcompat@mozilla.org"
    internal const val WEBCOMPAT_EXTENSION_URL = "resource://android/assets/extensions/webcompat/"

    /**
     * Installs the web extension in the runtime through the WebExtensionRuntime install method
     */
    fun install(runtime: WebExtensionRuntime) {
        runtime.installWebExtension(
            WEBCOMPAT_EXTENSION_ID,
            WEBCOMPAT_EXTENSION_URL,
            onSuccess = {
                logger.debug("Installed WebCompat webextension: ${it.id}")
            },
            onError = { ext, throwable ->
                logger.error("Failed to install WebCompat webextension: $ext", throwable)
            },
        )
    }
}
