/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.WebAppManifest

/**
 * Checks if the current session represents an installable web app.
 * If so, return the web app manifest. Otherwise, return null.
 *
 * Websites are installable if:
 * - The site is served over HTTPS
 * - The site has a valid manifest with a name or short_name
 * - The icons array in the manifest contains an icon of at least 192x192
 */
fun Session.installableManifest(): WebAppManifest? {
    val manifest = webAppManifest ?: return null
    return if (securityInfo.secure && manifest.isInstallable()) manifest else null
}
