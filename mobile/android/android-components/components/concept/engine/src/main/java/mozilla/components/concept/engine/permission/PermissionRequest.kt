/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.permission

/**
 * Represents a permission request, used when engines need access to protected
 * resources. Every request must be handled by either calling [grant] or [reject].
 */
interface PermissionRequest {
    /**
     * The origin URI which caused the permissions to be requested.
     */
    val uri: String?

    /**
     * A unique identifier for the request.
     */
    val id: String

    /**
     * List of requested permissions.
     */
    val permissions: List<Permission>

    /**
     * Grants the provided permissions, or all requested permissions, if none
     * are provided.
     *
     * @param permissions the permissions to grant.
     */
    fun grant(permissions: List<Permission> = this.permissions)

    /**
     * Grants this permission request if the provided predicate is true
     * for any of the requested permissions.
     *
     * @param predicate predicate to test for.
     * @return true if the permission request was granted, otherwise false.
     */
    fun grantIf(predicate: (Permission) -> Boolean): Boolean {
        return if (permissions.any(predicate)) {
            this.grant()
            true
        } else {
            false
        }
    }

    /**
     * Rejects the requested permissions.
     */
    fun reject()

    fun containsVideoAndAudioSources() = false
}

/**
 * Represents all the different supported permission types.
 *
 * @property id an optional native engine-specific ID of this permission.
 * @property desc an optional description of what this permission type is for.
 * @property name permission name allowing to easily identify and differentiate one from the other.
 */
@Suppress("UndocumentedPublicClass")
sealed class Permission {
    abstract val id: String?
    abstract val desc: String?
    val name: String = with(this::class.java) {
        // Using the canonicalName is safer - see https://github.com/mozilla-mobile/android-components/pull/10810
        // simpleName is used as a backup to the avoid not null assertion (!!) operator.
        canonicalName?.substringAfterLast('.') ?: simpleName
    }

    data class ContentAudioCapture(
        override val id: String? = "ContentAudioCapture",
        override val desc: String? = "",
    ) : Permission()
    data class ContentAudioMicrophone(
        override val id: String? = "ContentAudioMicrophone",
        override val desc: String? = "",
    ) : Permission()
    data class ContentAudioOther(
        override val id: String? = "ContentAudioOther",
        override val desc: String? = "",
    ) : Permission()
    data class ContentGeoLocation(
        override val id: String? = "ContentGeoLocation",
        override val desc: String? = "",
    ) : Permission()
    data class ContentNotification(
        override val id: String? = "ContentNotification",
        override val desc: String? = "",
    ) : Permission()
    data class ContentProtectedMediaId(
        override val id: String? = "ContentProtectedMediaId",
        override val desc: String? = "",
    ) : Permission()
    data class ContentVideoCamera(
        override val id: String? = "ContentVideoCamera",
        override val desc: String? = "",
    ) : Permission()
    data class ContentVideoCapture(
        override val id: String? = "ContentVideoCapture",
        override val desc: String? = "",
    ) : Permission()
    data class ContentVideoScreen(
        override val id: String? = "ContentVideoScreen",
        override val desc: String? = "",
    ) : Permission()
    data class ContentVideoOther(
        override val id: String? = "ContentVideoOther",
        override val desc: String? = "",
    ) : Permission()
    data class ContentAutoPlayAudible(
        override val id: String? = "ContentAutoPlayAudible",
        override val desc: String? = "",
    ) : Permission()
    data class ContentAutoPlayInaudible(
        override val id: String? = "ContentAutoPlayInaudible",
        override val desc: String? = "",
    ) : Permission()
    data class ContentPersistentStorage(
        override val id: String? = "ContentPersistentStorage",
        override val desc: String? = "",
    ) : Permission()
    data class ContentMediaKeySystemAccess(
        override val id: String? = "ContentMediaKeySystemAccess",
        override val desc: String? = "",
    ) : Permission()
    data class ContentCrossOriginStorageAccess(
        override val id: String? = "ContentCrossOriginStorageAccess",
        override val desc: String? = "",
    ) : Permission()

    data class AppCamera(
        override val id: String? = "AppCamera",
        override val desc: String? = "",
    ) : Permission()
    data class AppAudio(
        override val id: String? = "AppAudio",
        override val desc: String? = "",
    ) : Permission()
    data class AppLocationCoarse(
        override val id: String? = "AppLocationCoarse",
        override val desc: String? = "",
    ) : Permission()
    data class AppLocationFine(
        override val id: String? = "AppLocationFine",
        override val desc: String? = "",
    ) : Permission()

    data class Generic(
        override val id: String? = "Generic",
        override val desc: String? = "",
    ) : Permission()
}
