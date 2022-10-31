/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.concept.engine.permission.SitePermissions

/**
 * Internal entity representing a site permission as it gets saved to the database.
 */
@Entity(tableName = "site_permissions")
internal data class SitePermissionsEntity(

    @PrimaryKey
    @ColumnInfo(name = "origin")
    var origin: String,

    @ColumnInfo(name = "location")
    var location: SitePermissions.Status,

    @ColumnInfo(name = "notification")
    var notification: SitePermissions.Status,

    @ColumnInfo(name = "microphone")
    var microphone: SitePermissions.Status,

    @ColumnInfo(name = "camera")
    var camera: SitePermissions.Status,

    @ColumnInfo(name = "bluetooth")
    var bluetooth: SitePermissions.Status,

    @ColumnInfo(name = "local_storage")
    var localStorage: SitePermissions.Status,

    @ColumnInfo(name = "autoplay_audible")
    var autoplayAudible: SitePermissions.AutoplayStatus,

    @ColumnInfo(name = "autoplay_inaudible")
    var autoplayInaudible: SitePermissions.AutoplayStatus,

    @ColumnInfo(name = "media_key_system_access")
    var mediaKeySystemAccess: SitePermissions.Status,

    @ColumnInfo(name = "cross_origin_storage_access")
    var crossOriginStorageAccess: SitePermissions.Status,

    @ColumnInfo(name = "saved_at")
    var savedAt: Long,
) {

    internal fun toSitePermission(): SitePermissions {
        return SitePermissions(
            origin,
            location,
            notification,
            microphone,
            camera,
            bluetooth,
            localStorage,
            autoplayAudible,
            autoplayInaudible,
            mediaKeySystemAccess,
            crossOriginStorageAccess,
            savedAt,
        )
    }
}

internal fun SitePermissions.toSitePermissionsEntity(): SitePermissionsEntity {
    return SitePermissionsEntity(
        origin,
        location,
        notification,
        microphone,
        camera,
        bluetooth,
        localStorage,
        autoplayAudible,
        autoplayInaudible,
        mediaKeySystemAccess,
        crossOriginStorageAccess,
        savedAt,
    )
}
