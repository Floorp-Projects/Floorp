/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

/**
 * Value type that represents the state of a container also known as a contextual identity.
 *
 * @property contextId The session context ID also known as cookie store ID for the container.
 * @property name Name of the container.
 * @property color The color for the container. This can be shown in tabs belonging to this container.
 * @property icon The icon for the container.
 */
data class ContainerState(
    val contextId: String,
    val name: String,
    val color: Color,
    val icon: Icon,
) {
    /**
     * Enum of container color.
     */
    enum class Color(val color: String) {
        BLUE("blue"),
        TURQUOISE("turquoise"),
        GREEN("green"),
        YELLOW("yellow"),
        ORANGE("orange"),
        RED("red"),
        PINK("pink"),
        PURPLE("purple"),
        TOOLBAR("toolbar"),
    }

    /**
     * Enum of container icon.
     */
    enum class Icon(val icon: String) {
        FINGERPRINT("fingerprint"),
        BRIEFCASE("briefcase"),
        DOLLAR("dollar"),
        CART("cart"),
        CIRCLE("circle"),
        GIFT("gift"),
        VACATION("vacation"),
        FOOD("food"),
        FRUIT("fruit"),
        PET("pet"),
        TREE("tree"),
        CHILL("chill"),
        FENCE("fence"),
    }
}

typealias Container = ContainerState
