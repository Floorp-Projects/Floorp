/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.db

import androidx.room.Embedded
import androidx.room.Relation

/**
 * Class representing a [TabCollectionEntity] joined with its [TabEntity] instances.
 */
internal class TabCollectionWithTabs {
    @Embedded
    lateinit var collection: TabCollectionEntity

    @Relation(parentColumn = "id", entityColumn = "tab_collection_id", entity = TabEntity::class)
    lateinit var tabs: List<TabEntity>
}
