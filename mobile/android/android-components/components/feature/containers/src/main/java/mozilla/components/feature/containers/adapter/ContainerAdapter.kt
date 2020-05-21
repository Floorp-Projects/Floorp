/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers.adapter

import mozilla.components.feature.containers.Container
import mozilla.components.feature.containers.db.ContainerEntity

internal class ContainerAdapter(
    internal val entity: ContainerEntity
) : Container {
    override val contextId: String
        get() = entity.contextId

    override val name: String
        get() = entity.name

    override val color: String
        get() = entity.color

    override val icon: String
        get() = entity.icon

    override fun equals(other: Any?): Boolean {
        if (other !is ContainerAdapter) {
            return false
        }

        return entity == other.entity
    }

    override fun hashCode(): Int {
        return entity.hashCode()
    }
}
