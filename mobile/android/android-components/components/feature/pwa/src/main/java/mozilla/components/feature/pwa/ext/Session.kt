/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.WebAppManifest.Icon.Purpose
import kotlin.math.min

private const val MIN_INSTALLABLE_ICON_SIZE = 192

/**
 * Checks if the current session represents an installable web app.
 *
 * Websites are installable if:
 * - The site is served over HTTPS
 * - The site has a valid manifest with a name or short_name
 * - The icons array in the manifest contains an icon of at least 192x192
 */
fun Session.isInstallable(): Boolean {
    if (!securityInfo.secure) return false
    val manifest = webAppManifest ?: return false

    return manifest.icons.any { icon ->
        (Purpose.ANY in icon.purpose || Purpose.MASKABLE in icon.purpose) &&
            icon.sizes.any { size ->
                min(size.width, size.height) >= MIN_INSTALLABLE_ICON_SIZE
            }
    }
}
