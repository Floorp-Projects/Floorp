/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.os.Bundle
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.concept.engine.manifest.getOrNull

internal const val EXTRA_WEB_APP_MANIFEST = "mozilla.components.feature.pwa.EXTRA_WEB_APP_MANIFEST"

/**
 * Serializes and inserts a [WebAppManifest] value into the mapping of this [Bundle],
 * replacing any existing web app manifest.
 */
fun Bundle.putWebAppManifest(webAppManifest: WebAppManifest?) {
    val json = webAppManifest?.let { WebAppManifestParser().serialize(it).toString() }
    putString(EXTRA_WEB_APP_MANIFEST, json)
}

/**
 * Parses and returns the [WebAppManifest] associated with this [Bundle],
 * or null if no mapping of the desired type exists.
 */
fun Bundle.getWebAppManifest(): WebAppManifest? {
    return getString(EXTRA_WEB_APP_MANIFEST)?.let { json ->
        WebAppManifestParser().parse(json).getOrNull()
    }
}
