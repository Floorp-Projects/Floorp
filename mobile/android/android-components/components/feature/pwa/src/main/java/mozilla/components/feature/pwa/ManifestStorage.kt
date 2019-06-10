/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import androidx.core.util.readText
import androidx.core.util.writeText
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.support.ktx.kotlin.sha1
import java.io.File
import java.io.IOException

internal class ManifestStorage(context: Context) {
    private val filesDir = context.filesDir
    private val parser = WebAppManifestParser()

    /**
     * Load a Web App Manifest for the given URL from disk.
     * If no manifest is found, null is returned.
     *
     * @param startUrl URL of site. Should correspond to manifest's start_url.
     */
    suspend fun loadManifest(startUrl: String): WebAppManifest? = withContext(Dispatchers.IO) {
        val json = synchronized(manifestFileLock) {
            try {
                getFileForManifest(startUrl).readText()
            } catch (e: IOException) {
                ""
            }
        }

        when (val result = parser.parse(json)) {
            is WebAppManifestParser.Result.Success -> result.manifest
            is WebAppManifestParser.Result.Failure -> null
        }
    }

    /**
     * Save a Web App Manifest to disk.
     */
    suspend fun saveManifest(manifest: WebAppManifest) = withContext(Dispatchers.IO) {
        val json = parser.serialize(manifest).toString()
        synchronized(manifestFileLock) {
            getFileForManifest(manifest.startUrl).writeText(json)
        }
    }

    /**
     * Delete all manifests associated with the list of URLs.
     */
    suspend fun removeManifests(startUrls: List<String>) = withContext(Dispatchers.IO) {
        synchronized(manifestFileLock) {
            startUrls.forEach { getFileForManifest(it).delete() }
        }
    }

    /**
     * Get a file to store a single Web App Manifest.
     * The URL that the manifest corresponds to is used as a key for the filename.
     *
     * @param startUrl start_url from manifest.
     */
    @VisibleForTesting
    internal fun getFileForManifest(startUrl: String): AtomicFile {
        val filename = String.format(STORE_FILE_NAME_FORMAT, startUrl.sha1())
        return AtomicFile(File(filesDir, filename))
    }

    companion object {
        private const val STORE_FILE_NAME_FORMAT = "mozilla_components_manifest_storage_%s.json"
        private val manifestFileLock = Any()
    }
}
