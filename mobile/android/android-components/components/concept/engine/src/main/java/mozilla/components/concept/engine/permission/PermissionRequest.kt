/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.permission

import android.os.Parcelable
import kotlinx.android.parcel.Parcelize

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
 */

sealed class Permission(open val id: String? = "", open val desc: String? = "") : Parcelable {
    @Parcelize
    data class ContentAudioCapture(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentAudioMicrophone(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentAudioOther(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentAutoplayMedia(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentGeoLocation(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentNotification(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentProtectedMediaId(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentVideoApplication(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentVideoBrowser(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentVideoCamera(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentVideoCapture(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentVideoScreen(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentVideoWindow(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class ContentVideoOther(override val id: String? = "", override val desc: String? = "") : Permission(id)

    @Parcelize
    data class AppCamera(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class AppAudio(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class AppLocationCoarse(override val id: String? = "", override val desc: String? = "") : Permission(id)
    @Parcelize
    data class AppLocationFine(override val id: String? = "", override val desc: String? = "") : Permission(id)

    @Parcelize
    data class Generic(override val id: String?, override val desc: String? = "") : Permission(id)
}
