/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.concept.engine.manifest.WebAppManifest

/**
 * Internal entity representing a web app manifest.
 */
@Entity(tableName = "manifests")
internal data class ManifestEntity(
    val manifest: WebAppManifest,

    @PrimaryKey
    @ColumnInfo(name = "start_url")
    val startUrl: String,

    @ColumnInfo(name = "scope", index = true)
    val scope: String?,

    @ColumnInfo(name = "has_share_targets", index = true)
    val hasShareTargets: Int,

    @ColumnInfo(name = "created_at")
    val createdAt: Long,

    @ColumnInfo(name = "updated_at")
    val updatedAt: Long,

    @ColumnInfo(name = "used_at")
    val usedAt: Long,
) {
    constructor(
        manifest: WebAppManifest,
        currentTime: Long = System.currentTimeMillis(),
    ) : this(
        manifest,
        startUrl = manifest.startUrl,
        scope = manifest.scope,
        hasShareTargets = if (manifest.shareTarget != null) 1 else 0,
        createdAt = currentTime,
        updatedAt = currentTime,
        usedAt = currentTime,
    )
}
