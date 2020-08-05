/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.adapter

import mozilla.components.feature.top.sites.PinnedSite
import mozilla.components.feature.top.sites.db.PinnedSiteEntity

internal class PinnedSiteAdapter(
    internal val entity: PinnedSiteEntity
) : PinnedSite {
    override val id: Long
        get() = entity.id!!

    override val title: String
        get() = entity.title

    override val url: String
        get() = entity.url

    override val isDefault: Boolean
        get() = entity.isDefault

    override fun equals(other: Any?): Boolean {
        if (other !is PinnedSiteAdapter) {
            return false
        }

        return entity == other.entity
    }

    override fun hashCode(): Int {
        return entity.hashCode()
    }
}
