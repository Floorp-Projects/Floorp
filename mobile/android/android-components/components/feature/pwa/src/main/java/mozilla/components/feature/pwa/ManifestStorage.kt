/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.db.ManifestDatabase
import mozilla.components.feature.pwa.db.ManifestEntity

/**
 * Disk storage for [WebAppManifest]. Other components use this class to reload a saved manifest.
 *
 * @param context the application context this storage is associated with
 * @param activeThresholdMs a timeout in milliseconds after which the storage will consider a manifest
 *                      as unused. By default this is [ACTIVE_THRESHOLD_MS].
 */
class ManifestStorage(context: Context, private val activeThresholdMs: Long = ACTIVE_THRESHOLD_MS) {

    @VisibleForTesting
    internal var manifestDao = lazy { ManifestDatabase.get(context).manifestDao() }
    internal var installedScopes: MutableMap<String, String>? = null

    /**
     * Load a Web App Manifest for the given URL from disk.
     * If no manifest is found, null is returned.
     *
     * @param startUrl URL of site. Should correspond to manifest's start_url.
     */
    suspend fun loadManifest(startUrl: String): WebAppManifest? = withContext(IO) {
        manifestDao.value.getManifest(startUrl)?.manifest
    }

    /**
     * Load all Web App Manifests with a matching scope for the given URL from disk.
     * If no manifests are found, an empty list is returned.
     *
     * @param url URL of site. Should correspond to an url covered by the scope of a stored manifest.
     */
    suspend fun loadManifestsByScope(url: String): List<WebAppManifest> = withContext(IO) {
        manifestDao.value.getManifestsByScope(url).map { it.manifest }
    }

    /**
     * Checks whether there is a currently used manifest with a scope that matches the url.
     *
     * @param url the url to match with manifest scopes.
     * @param currentTimeMs the current time in milliseconds.
     */
    suspend fun hasRecentManifest(
        url: String,
        @VisibleForTesting currentTimeMs: Long = System.currentTimeMillis(),
    ): Boolean = withContext(IO) {
        manifestDao.value.hasRecentManifest(url, thresholdMs = currentTimeMs - activeThresholdMs) > 0
    }

    /**
     * Counts number of recently used manifests, as configured by [activeThresholdMs].
     *
     * @param currentTimeMs current time, exposed for testing
     * @param activeThresholdMs a time threshold within which manifests are considered to be recently used.
     */
    suspend fun recentManifestsCount(
        activeThresholdMs: Long = this.activeThresholdMs,
        @VisibleForTesting currentTimeMs: Long = System.currentTimeMillis(),
    ): Int = withContext(IO) {
        manifestDao.value.recentManifestsCount(thresholdMs = currentTimeMs - activeThresholdMs)
    }

    /**
     * Returns the cached scope for an url if the url falls into a web app scope that has been installed by the user.
     *
     * @param url the url to match against installed web app scopes.
     */
    fun getInstalledScope(url: String) = installedScopes?.keys?.sortedDescending()?.find { url.startsWith(it) }

    /**
     * Returns a cached start url for an installed web app scope.
     *
     * @param scope the scope url to look up.
     */
    fun getStartUrlForInstalledScope(scope: String) = installedScopes?.get(scope)

    /**
     * Populates a cache of currently installed web app scopes and their start urls.
     *
     * @param currentTime the current time is used to determine which web apps are still installed.
     */
    suspend fun warmUpScopes(currentTime: Long) = withContext(IO) {
        installedScopes = manifestDao.value
            .getInstalledScopes(deadline(currentTime))
            .mapNotNull { manifest -> manifest.scope?.let { scope -> Pair(scope, manifest.startUrl) } }
            .toMap()
            .toMutableMap()
    }

    /**
     * Load all Web App Manifests that contain share targets.
     * If no manifests are found, an empty list is returned.
     *
     * @param currentTime the current time in milliseconds.
     */
    suspend fun loadShareableManifests(currentTime: Long): List<WebAppManifest> = withContext(IO) {
        manifestDao.value.getRecentShareableManifests(deadline(currentTime)).map { it.manifest }
    }

    /**
     * Save a Web App Manifest to disk.
     */
    suspend fun saveManifest(manifest: WebAppManifest) = withContext(IO) {
        val entity = ManifestEntity(manifest, currentTime = System.currentTimeMillis())
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
     * Update the last time a web app was used.
     *
     * @param manifest the manifest to update
     */
    suspend fun updateManifestUsedAt(manifest: WebAppManifest) = withContext(IO) {
        manifestDao.value.getManifest(manifest.startUrl)?.let { existing ->
            val update = existing.copy(usedAt = System.currentTimeMillis())
            manifestDao.value.updateManifest(update)

            existing.scope?.let { scope ->
                installedScopes?.put(scope, existing.startUrl)
            }

            return@let
        }
    }

    /**
     * Delete all manifests associated with the list of URLs.
     */
    suspend fun removeManifests(startUrls: List<String>) = withContext(IO) {
        manifestDao.value.deleteManifests(startUrls)
    }

    private fun deadline(currentTime: Long) = currentTime - activeThresholdMs

    companion object {
        const val ACTIVE_THRESHOLD_MS = 86400000 * 30L // 30 days
    }
}
