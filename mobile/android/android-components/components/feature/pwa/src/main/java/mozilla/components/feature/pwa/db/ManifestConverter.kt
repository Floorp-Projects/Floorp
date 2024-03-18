/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.db

import androidx.room.TypeConverter
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifestParser

/**
 * Converts a web app manifest to and from JSON strings
 */
internal class ManifestConverter {
    private val parser = WebAppManifestParser()

    @TypeConverter
    fun fromJsonString(json: String): WebAppManifest =
        when (val result = parser.parse(json)) {
            is WebAppManifestParser.Result.Success -> result.manifest
            is WebAppManifestParser.Result.Failure -> throw result.exception
        }

    @TypeConverter
    fun toJsonString(manifest: WebAppManifest): String = parser.serialize(manifest).toString()
}
