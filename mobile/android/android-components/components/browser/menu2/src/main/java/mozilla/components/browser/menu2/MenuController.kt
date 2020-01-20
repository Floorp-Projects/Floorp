/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2

/**
 * Controls a popup menu composed of MenuCandidate objects.
 * @param visibleSide Sets the menu to open with either the start or end visible.
 */
class MenuController(
    private val visibleSide: Side = Side.START
)

/**
 * Indicates the starting or ending side of the menu or an option.
 */
enum class Side {
    /**
     * Starting side (top or left).
     */
    START,
    /**
     * Ending side (bottom or right).
     */
    END
}
