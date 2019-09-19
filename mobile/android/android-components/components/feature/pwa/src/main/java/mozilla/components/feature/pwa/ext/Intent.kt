/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.content.Intent
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifestParser

/**
 * Add extended [WebAppManifest] data to the intent.
 */
fun Intent.putWebAppManifest(webAppManifest: WebAppManifest) {
    val json = WebAppManifestParser().serialize(webAppManifest)
    putExtra(EXTRA_WEB_APP_MANIFEST, json.toString())
}

/**
 * Retrieve extended [WebAppManifest] data from the intent.
 */
fun Intent.getWebAppManifest(): WebAppManifest? {
    return extras?.getWebAppManifest()
}
