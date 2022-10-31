/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.content.Intent
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifestParser

internal const val EXTRA_URL_OVERRIDE = "mozilla.components.feature.pwa.EXTRA_URL_OVERRIDE"

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

/**
 * Add [String] URL override to the intent.
 *
 * @param url The URL override value.
 *
 * @return Returns the same Intent object, for chaining multiple calls
 * into a single statement.
 *
 * @see [getUrlOverride]
 */
fun Intent.putUrlOverride(url: String?): Intent {
    return putExtra(EXTRA_URL_OVERRIDE, url)
}

/**
 * Retrieves [String] Url override from the intent.
 *
 * @return The URL override previously added with [putUrlOverride],
 * or null if no URL was found.
 */
fun Intent.getUrlOverride(): String? = getStringExtra(EXTRA_URL_OVERRIDE)
