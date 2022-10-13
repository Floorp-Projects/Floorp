/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.permission

import androidx.annotation.VisibleForTesting
import androidx.paging.DataSource
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.engine.gecko.await
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.support.ktx.kotlin.stripDefaultPort
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_PROMPT
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_AUTOPLAY_AUDIBLE
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_AUTOPLAY_INAUDIBLE
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_MEDIA_KEY_SYSTEM_ACCESS
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_PERSISTENT_STORAGE
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_STORAGE_ACCESS
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_TRACKING
import org.mozilla.geckoview.StorageController
import org.mozilla.geckoview.StorageController.ClearFlags

/**
 * A storage to save [SitePermissions] using GeckoView APIs.
 */
@Suppress("LargeClass")
class GeckoSitePermissionsStorage(
    runtime: GeckoRuntime,
    private val onDiskStorage: SitePermissionsStorage,
) : SitePermissionsStorage {

    private val geckoStorage: StorageController = runtime.storageController
    private val mainScope = CoroutineScope(Dispatchers.Main)

    /*
    * Temporary permissions are created when users doesn't
    * check the 'Remember my decision checkbox'.  At the moment,
    *  gecko view doesn't handle temporary permission,
    * we have to store them in memory, and clear them manually,
    * until we have an API for it see:
    * https://bugzilla.mozilla.org/show_bug.cgi?id=1710447
    * */
    @VisibleForTesting
    internal val geckoTemporaryPermissions = mutableListOf<ContentPermission>()

    override suspend fun save(sitePermissions: SitePermissions, request: PermissionRequest?) {
        val geckoSavedPermissions = updateGeckoPermissionIfNeeded(sitePermissions, request)
        onDiskStorage.save(geckoSavedPermissions, request)
    }

    override fun saveTemporary(request: PermissionRequest?) {
        if (request is GeckoPermissionRequest.Content) {
            geckoTemporaryPermissions.add(request.geckoPermission)
        }
    }

    override fun clearTemporaryPermissions() {
        geckoTemporaryPermissions.forEach {
            geckoStorage.setPermission(it, VALUE_PROMPT)
        }
        geckoTemporaryPermissions.clear()
    }

    override suspend fun update(sitePermissions: SitePermissions) {
        val updatedPermission = updateGeckoPermissionIfNeeded(sitePermissions)
        onDiskStorage.update(updatedPermission)
    }

    override suspend fun findSitePermissionsBy(origin: String, includeTemporary: Boolean): SitePermissions? {
        /**
         * GeckoView ony persists [GeckoPermissionRequest.Content] other permissions like
         * [GeckoPermissionRequest.Media], we have to store them ourselves.
         * For this reason, we query both storage ([geckoStorage] and [onDiskStorage]) and
         * merge both results into one [SitePermissions] object.
         */
        val onDiskPermission: SitePermissions? = onDiskStorage.findSitePermissionsBy(origin)
        val geckoPermissions = findGeckoContentPermissionBy(origin, includeTemporary).groupByType()

        return mergePermissions(onDiskPermission, geckoPermissions)
    }

    override suspend fun getSitePermissionsPaged(): DataSource.Factory<Int, SitePermissions> {
        val geckoPermissionsByHost = findAllGeckoContentPermissions().groupByDomain()

        return onDiskStorage.getSitePermissionsPaged().map { onDiskPermission ->
            val geckoPermissions = geckoPermissionsByHost[onDiskPermission.origin].groupByType()
            mergePermissions(onDiskPermission, geckoPermissions)
        }
    }

    override suspend fun remove(sitePermissions: SitePermissions) {
        onDiskStorage.remove(sitePermissions)
        removeGeckoContentPermissionBy(sitePermissions.origin)
    }

    override suspend fun removeAll() {
        onDiskStorage.removeAll()
        removeGeckoAllContentPermissions()
    }

    override suspend fun all(): List<SitePermissions> {
        val onDiskPermissions: List<SitePermissions> = onDiskStorage.all()
        val geckoPermissionsByHost = findAllGeckoContentPermissions().groupByDomain()

        return onDiskPermissions.mapNotNull { onDiskPermission ->
            val map = geckoPermissionsByHost[onDiskPermission.origin].groupByType()
            mergePermissions(onDiskPermission, map)
        }
    }

    @VisibleForTesting
    internal suspend fun findAllGeckoContentPermissions(): List<ContentPermission>? {
        return withContext(mainScope.coroutineContext) {
            geckoStorage.allPermissions.await()
                .filterNotTemporaryPermissions(geckoTemporaryPermissions)
        }
    }

    /**
     * Updates the [geckoStorage] if the provided [userSitePermissions]
     * exists on the [geckoStorage] or it's provided as a part of the [permissionRequest]
     * otherwise nothing is updated.
     * @param userSitePermissions the values provided by the user to be updated.
     * @param permissionRequest the [PermissionRequest] from the web content.
     * @return An updated [SitePermissions] with default values, if they were updated
     * on the [geckoStorage] otherwise the same [SitePermissions].
     */
    @VisibleForTesting
    @Suppress("LongMethod")
    internal suspend fun updateGeckoPermissionIfNeeded(
        userSitePermissions: SitePermissions,
        permissionRequest: PermissionRequest? = null,
    ): SitePermissions {
        var updatedPermission = userSitePermissions
        val geckoPermissionsByType =
            permissionRequest.extractGeckoPermissionsOrQueryTheStore(userSitePermissions.origin)

        if (geckoPermissionsByType.isNotEmpty()) {
            val geckoNotification = geckoPermissionsByType[PERMISSION_DESKTOP_NOTIFICATION]?.firstOrNull()
            val geckoLocation = geckoPermissionsByType[PERMISSION_GEOLOCATION]?.firstOrNull()
            val geckoMedia = geckoPermissionsByType[PERMISSION_MEDIA_KEY_SYSTEM_ACCESS]?.firstOrNull()
            val geckoLocalStorage = geckoPermissionsByType[PERMISSION_PERSISTENT_STORAGE]?.firstOrNull()
            val geckoCrossOriginStorageAccess = geckoPermissionsByType[PERMISSION_STORAGE_ACCESS]?.firstOrNull()
            val geckoAudible = geckoPermissionsByType[PERMISSION_AUTOPLAY_AUDIBLE]?.firstOrNull()
            val geckoInAudible = geckoPermissionsByType[PERMISSION_AUTOPLAY_INAUDIBLE]?.firstOrNull()

            /*
            * To avoid GeckoView caching previous request, we need to clear, previous data
            * before updating. See: https://github.com/mozilla-mobile/android-components/issues/6322
            * */
            clearGeckoCacheFor(updatedPermission.origin)

            if (geckoNotification != null) {
                removeTemporaryPermissionIfAny(geckoNotification)
                geckoStorage.setPermission(
                    geckoNotification,
                    userSitePermissions.notification.toGeckoStatus(),
                )
                updatedPermission = updatedPermission.copy(notification = NO_DECISION)
            }

            if (geckoLocation != null) {
                removeTemporaryPermissionIfAny(geckoLocation)
                geckoStorage.setPermission(
                    geckoLocation,
                    userSitePermissions.location.toGeckoStatus(),
                )
                updatedPermission = updatedPermission.copy(location = NO_DECISION)
            }

            if (geckoMedia != null) {
                removeTemporaryPermissionIfAny(geckoMedia)
                geckoStorage.setPermission(
                    geckoMedia,
                    userSitePermissions.mediaKeySystemAccess.toGeckoStatus(),
                )
                updatedPermission = updatedPermission.copy(mediaKeySystemAccess = NO_DECISION)
            }

            if (geckoLocalStorage != null) {
                removeTemporaryPermissionIfAny(geckoLocalStorage)
                geckoStorage.setPermission(
                    geckoLocalStorage,
                    userSitePermissions.localStorage.toGeckoStatus(),
                )
                updatedPermission = updatedPermission.copy(localStorage = NO_DECISION)
            }

            if (geckoCrossOriginStorageAccess != null) {
                removeTemporaryPermissionIfAny(geckoCrossOriginStorageAccess)
                geckoStorage.setPermission(
                    geckoCrossOriginStorageAccess,
                    userSitePermissions.crossOriginStorageAccess.toGeckoStatus(),
                )
                updatedPermission = updatedPermission.copy(crossOriginStorageAccess = NO_DECISION)
            }

            if (geckoAudible != null) {
                removeTemporaryPermissionIfAny(geckoAudible)
                geckoStorage.setPermission(
                    geckoAudible,
                    userSitePermissions.autoplayAudible.toGeckoStatus(),
                )
                updatedPermission = updatedPermission.copy(autoplayAudible = AutoplayStatus.BLOCKED)
            }

            if (geckoInAudible != null) {
                removeTemporaryPermissionIfAny(geckoInAudible)
                geckoStorage.setPermission(
                    geckoInAudible,
                    userSitePermissions.autoplayInaudible.toGeckoStatus(),
                )
                updatedPermission =
                    updatedPermission.copy(autoplayInaudible = AutoplayStatus.BLOCKED)
            }
        }
        return updatedPermission
    }

    /**
     * Combines a permission that comes from our on disk storage with the gecko permissions,
     * and combined both into a single a [SitePermissions].
     * @param onDiskPermissions a permission from the on disk storage.
     * @param geckoPermissionByType a list of all the gecko permissions mapped by permission type.
     * @return a [SitePermissions] containing the values from the on disk and gecko permission.
     */
    @VisibleForTesting
    @Suppress("ComplexMethod")
    internal fun mergePermissions(
        onDiskPermissions: SitePermissions?,
        geckoPermissionByType: Map<Int, List<ContentPermission>>,
    ): SitePermissions? {
        var combinedPermissions = onDiskPermissions

        if (geckoPermissionByType.isNotEmpty() && onDiskPermissions != null) {
            val geckoNotification = geckoPermissionByType[PERMISSION_DESKTOP_NOTIFICATION]?.firstOrNull()
            val geckoLocation = geckoPermissionByType[PERMISSION_GEOLOCATION]?.firstOrNull()
            val geckoMedia = geckoPermissionByType[PERMISSION_MEDIA_KEY_SYSTEM_ACCESS]?.firstOrNull()
            val geckoStorage = geckoPermissionByType[PERMISSION_PERSISTENT_STORAGE]?.firstOrNull()
            // Currently we'll receive the "storage_access" permission for all iframes of the same parent
            // so we need to ensure we are reporting the permission for the current iframe request.
            // See https://bugzilla.mozilla.org/show_bug.cgi?id=1746436 for more details.
            val geckoCrossOriginStorageAccess = geckoPermissionByType[PERMISSION_STORAGE_ACCESS]?.firstOrNull {
                it.thirdPartyOrigin == onDiskPermissions.origin.stripDefaultPort()
            }
            val geckoAudible = geckoPermissionByType[PERMISSION_AUTOPLAY_AUDIBLE]?.firstOrNull()
            val geckoInAudible = geckoPermissionByType[PERMISSION_AUTOPLAY_INAUDIBLE]?.firstOrNull()

            /**
             * We only consider permissions from geckoView, when the values default value
             * has been changed otherwise we favor the values [onDiskPermissions].
             */
            if (geckoNotification != null && geckoNotification.value != VALUE_PROMPT) {
                combinedPermissions = combinedPermissions?.copy(
                    notification = geckoNotification.value.toStatus(),
                )
            }

            if (geckoLocation != null && geckoLocation.value != VALUE_PROMPT) {
                combinedPermissions = combinedPermissions?.copy(
                    location = geckoLocation.value.toStatus(),
                )
            }

            if (geckoMedia != null && geckoMedia.value != VALUE_PROMPT) {
                combinedPermissions = combinedPermissions?.copy(
                    mediaKeySystemAccess = geckoMedia.value.toStatus(),
                )
            }

            if (geckoStorage != null && geckoStorage.value != VALUE_PROMPT) {
                combinedPermissions = combinedPermissions?.copy(
                    localStorage = geckoStorage.value.toStatus(),
                )
            }

            if (geckoCrossOriginStorageAccess != null && geckoCrossOriginStorageAccess.value != VALUE_PROMPT) {
                combinedPermissions = combinedPermissions?.copy(
                    crossOriginStorageAccess = geckoCrossOriginStorageAccess.value.toStatus(),
                )
            }

            /**
             * Autoplay permissions don't have initial values, so when the value is changed on
             * the gecko storage we trust it.
             */
            if (geckoAudible != null && geckoAudible.value != VALUE_PROMPT) {
                combinedPermissions = combinedPermissions?.copy(
                    autoplayAudible = geckoAudible.value.toAutoPlayStatus(),
                )
            }

            if (geckoInAudible != null && geckoInAudible.value != VALUE_PROMPT) {
                combinedPermissions = combinedPermissions?.copy(
                    autoplayInaudible = geckoInAudible.value.toAutoPlayStatus(),
                )
            }
        }
        return combinedPermissions
    }

    @VisibleForTesting
    internal suspend fun findGeckoContentPermissionBy(
        origin: String,
        includeTemporary: Boolean = false,
    ): List<ContentPermission>? {
        return withContext(mainScope.coroutineContext) {
            val geckoPermissions = geckoStorage.getPermissions(origin).await()

            if (includeTemporary) {
                geckoPermissions
            } else {
                geckoPermissions.filterNotTemporaryPermissions(geckoTemporaryPermissions)
            }
        }
    }

    @VisibleForTesting
    internal suspend fun clearGeckoCacheFor(origin: String) {
        withContext(mainScope.coroutineContext) {
            geckoStorage.clearDataFromHost(origin, ClearFlags.PERMISSIONS).await()
        }
    }

    @VisibleForTesting
    internal fun clearAllPermissionsGeckoCache() {
        geckoStorage.clearData(ClearFlags.PERMISSIONS)
    }

    @VisibleForTesting
    internal fun removeTemporaryPermissionIfAny(permission: ContentPermission) {
        if (geckoTemporaryPermissions.any { permission.areSame(it) }) {
            geckoTemporaryPermissions.removeAll { permission.areSame(it) }
        }
    }

    @VisibleForTesting
    internal suspend fun removeGeckoContentPermissionBy(origin: String) {
        findGeckoContentPermissionBy(origin)?.forEach { geckoPermissions ->
            removeGeckoContentPermission(geckoPermissions)
        }
    }

    @VisibleForTesting
    internal fun removeGeckoContentPermission(geckoPermissions: ContentPermission) {
        val value = if (geckoPermissions.permission != PERMISSION_TRACKING) {
            VALUE_PROMPT
        } else {
            VALUE_DENY
        }
        geckoStorage.setPermission(geckoPermissions, value)
        removeTemporaryPermissionIfAny(geckoPermissions)
    }

    @VisibleForTesting
    internal suspend fun removeGeckoAllContentPermissions() {
        findAllGeckoContentPermissions()?.forEach { geckoPermissions ->
            removeGeckoContentPermission(geckoPermissions)
        }
        clearAllPermissionsGeckoCache()
    }

    private suspend fun PermissionRequest?.extractGeckoPermissionsOrQueryTheStore(
        origin: String,
    ): Map<Int, List<ContentPermission>> {
        return if (this is GeckoPermissionRequest.Content) {
            mapOf(geckoPermission.permission to listOf(geckoPermission))
        } else {
            findGeckoContentPermissionBy(origin, includeTemporary = true).groupByType()
        }
    }
}

@VisibleForTesting
internal fun List<ContentPermission>?.groupByDomain(): Map<String, List<ContentPermission>> {
    return this?.groupBy {
        it.uri.tryGetHostFromUrl()
    }.orEmpty()
}

@VisibleForTesting
internal fun List<ContentPermission>?.groupByType(): Map<Int, List<ContentPermission>> {
    return this?.groupBy { it.permission }.orEmpty()
}

@VisibleForTesting
internal fun List<ContentPermission>?.filterNotTemporaryPermissions(
    temporaryPermissions: List<ContentPermission>,
): List<ContentPermission>? {
    return this?.filterNot { geckoPermission ->
        temporaryPermissions.any { geckoPermission.areSame(it) }
    }
}

@VisibleForTesting
internal fun ContentPermission.areSame(other: ContentPermission) =
    other.uri.tryGetHostFromUrl() == this.uri.tryGetHostFromUrl() &&
        other.permission == this.permission

@VisibleForTesting
internal fun Int.toStatus(): Status {
    return when (this) {
        VALUE_PROMPT -> NO_DECISION
        VALUE_DENY -> BLOCKED
        VALUE_ALLOW -> ALLOWED
        else -> BLOCKED
    }
}

@VisibleForTesting
internal fun Int.toAutoPlayStatus(): AutoplayStatus {
    return when (this) {
        VALUE_PROMPT, VALUE_DENY -> AutoplayStatus.BLOCKED
        VALUE_ALLOW -> AutoplayStatus.ALLOWED
        else -> AutoplayStatus.BLOCKED
    }
}

@VisibleForTesting
internal fun Status.toGeckoStatus(): Int {
    return when (this) {
        NO_DECISION -> VALUE_PROMPT
        BLOCKED -> VALUE_DENY
        ALLOWED -> VALUE_ALLOW
        else -> VALUE_ALLOW
    }
}

@VisibleForTesting
internal fun AutoplayStatus.toGeckoStatus(): Int {
    return when (this) {
        AutoplayStatus.BLOCKED -> VALUE_DENY
        AutoplayStatus.ALLOWED -> VALUE_ALLOW
    }
}
