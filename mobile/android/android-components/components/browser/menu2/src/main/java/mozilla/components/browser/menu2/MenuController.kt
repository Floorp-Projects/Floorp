/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2

import mozilla.components.concept.menu.Side

/**
 * Controls a popup menu composed of MenuCandidate objects.
 * @param visibleSide Sets the menu to open with either the start or end visible.
 */
class MenuController(
    private val visibleSide: Side = Side.START
)
