/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifest.DisplayMode.BROWSER

/**
 * Checks if the current session represents an installable web app.
 * If so, return the web app manifest. Otherwise, return null.
 *
 * Websites are installable if:
 * - The site is served over HTTPS
 * - The site has a valid manifest with a name or short_name
 * - The manifest display mode is standalone, fullscreen, or minimal-ui
 * - The icons array in the manifest contains an icon of at least 192x192
 */
fun SessionState.installableManifest(): WebAppManifest? {
    val manifest = content.webAppManifest ?: return null
    return if (content.securityInfo.secure && manifest.display != BROWSER && manifest.hasLargeIcons()) {
        manifest
    } else {
        null
    }
}
