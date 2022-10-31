/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share.adapter

import mozilla.components.feature.share.RecentApp
import mozilla.components.feature.share.db.RecentAppEntity

internal class RecentAppAdapter(
    internal val entity: RecentAppEntity,
) : RecentApp {

    override val activityName: String
        get() = entity.activityName

    override val score: Double
        get() = entity.score

    override fun equals(other: Any?): Boolean {
        if (other !is RecentAppAdapter) {
            return false
        }
        return entity == other.entity
    }

    override fun hashCode(): Int {
        return entity.hashCode()
    }
}
