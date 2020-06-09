/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers

/**
 * A container also known as a contextual identity.
 */
interface Container {
    /**
     * The session context ID also known as cookie store ID for the container.
     */
    val contextId: String

    /**
     * Name of the container.
     */
    val name: String

    /**
     * The color for the container. This can be shown in tabs belonging to this container.
     */
    val color: String

    /**
     * The name of an icon for the container.
     */
    val icon: String
}
