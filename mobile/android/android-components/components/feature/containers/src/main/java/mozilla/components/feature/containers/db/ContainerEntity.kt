/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey

/**
 * Internal entity representing a container (contextual identity).
 */
@Entity(tableName = "containers")
internal data class ContainerEntity(
    @PrimaryKey
    @ColumnInfo(name = "context_id")
    var contextId: String,

    @ColumnInfo(name = "name")
    var name: String,

    @ColumnInfo(name = "color")
    var color: String,

    @ColumnInfo(name = "icon")
    var icon: String
)
