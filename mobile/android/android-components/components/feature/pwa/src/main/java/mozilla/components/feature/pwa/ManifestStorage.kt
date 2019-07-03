/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.db.ManifestDatabase
import mozilla.components.feature.pwa.db.ManifestEntity

class ManifestStorage(context: Context) {

    @VisibleForTesting
    internal var manifestDao = lazy { ManifestDatabase.get(context).manifestDao() }

    /**
     * Load a Web App Manifest for the given URL from disk.
     * If no manifest is found, null is returned.
     *
     * @param startUrl URL of site. Should correspond to manifest's start_url.
     */
    suspend fun loadManifest(startUrl: String): WebAppManifest? = withContext(Dispatchers.IO) {
        manifestDao.value.getManifest(startUrl)?.manifest
    }

    /**
     * Save a Web App Manifest to disk.
     */
    suspend fun saveManifest(manifest: WebAppManifest) = withContext(Dispatchers.IO) {
        val entity = ManifestEntity(
            manifest = manifest,
            updatedAt = System.currentTimeMillis(),
            createdAt = System.currentTimeMillis()
        )
        manifestDao.value.insertManifest(entity)
    }

    /**
     * Delete all manifests associated with the list of URLs.
     */
    suspend fun removeManifests(startUrls: List<String>) = withContext(Dispatchers.IO) {
        manifestDao.value.deleteManifests(startUrls)
    }
}
