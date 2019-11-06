/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.db.ManifestDatabase
import mozilla.components.feature.pwa.db.ManifestEntity

/**
 * Disk storage for [WebAppManifest]. Other components use this class to reload a saved manifest.
 */
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
    suspend fun saveManifest(manifest: WebAppManifest) = withContext(IO) {
        val entity = ManifestEntity(
            manifest = manifest,
            updatedAt = System.currentTimeMillis(),
            createdAt = System.currentTimeMillis()
        )
        manifestDao.value.insertManifest(entity)
    }

    /**
     * Update an existing Web App Manifest on disk.
     */
    suspend fun updateManifest(manifest: WebAppManifest) = withContext(IO) {
        manifestDao.value.getManifest(manifest.startUrl)?.let { existing ->
            val update = existing.copy(manifest = manifest, updatedAt = System.currentTimeMillis())
            manifestDao.value.updateManifest(update)
        }
    }

    /**
     * Delete all manifests associated with the list of URLs.
     */
    suspend fun removeManifests(startUrls: List<String>) = withContext(IO) {
        manifestDao.value.deleteManifests(startUrls)
    }
}
