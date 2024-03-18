/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.verify

import android.content.pm.PackageManager
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsService.RELATION_USE_AS_ORIGIN
import androidx.browser.customtabs.CustomTabsService.Relation
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import mozilla.components.service.digitalassetlinks.AndroidAssetFinder
import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation.HANDLE_ALL_URLS
import mozilla.components.service.digitalassetlinks.Relation.USE_AS_ORIGIN
import mozilla.components.service.digitalassetlinks.RelationChecker

/**
 * Used to verify postMessage origin for a designated package name.
 *
 * Uses Digital Asset Links to confirm that the given origin is associated with the package name.
 * It caches any origin that has been verified during the current application
 * lifecycle and reuses that without making any new network requests.
 */
class OriginVerifier(
    private val packageName: String,
    @Relation private val relation: Int,
    packageManager: PackageManager,
    private val relationChecker: RelationChecker,
) {

    @VisibleForTesting
    internal val androidAsset by lazy {
        AndroidAssetFinder().getAndroidAppAsset(packageName, packageManager).firstOrNull()
    }

    /**
     * Verify the claimed origin for the cached package name asynchronously. This will end up
     * making a network request for non-cached origins with a HTTP [Client].
     *
     * @param origin The postMessage origin the application is claiming to have. Can't be null.
     */
    suspend fun verifyOrigin(origin: Uri) = withContext(IO) { verifyOriginInternal(origin) }

    @Suppress("ReturnCount")
    private fun verifyOriginInternal(origin: Uri): Boolean {
        val cachedOrigin = cachedOriginMap[packageName]
        if (cachedOrigin == origin) return true

        if (origin.scheme != "https") return false
        val relationship = when (relation) {
            RELATION_USE_AS_ORIGIN -> USE_AS_ORIGIN
            RELATION_HANDLE_ALL_URLS -> HANDLE_ALL_URLS
            else -> return false
        }

        val originVerified = relationChecker.checkRelationship(
            source = AssetDescriptor.Web(site = origin.toString()),
            target = androidAsset ?: return false,
            relation = relationship,
        )

        if (originVerified && packageName !in cachedOriginMap) {
            cachedOriginMap[packageName] = origin
        }
        return originVerified
    }

    companion object {
        private val cachedOriginMap = mutableMapOf<String, Uri>()
    }
}
