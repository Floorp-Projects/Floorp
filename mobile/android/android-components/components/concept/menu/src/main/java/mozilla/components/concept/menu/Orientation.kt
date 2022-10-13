/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu

import android.view.Gravity

/**
 * Indicates the preferred orientation to show the menu.
 */
enum class Orientation {
    /**
     * Position the menu above the toolbar.
     */
    UP,

    /**
     * Position the menu below the toolbar.
     */
    DOWN,

    ;

    companion object {

        /**
         * Returns an orientation that matches the given [Gravity] value.
         * Meant to be used with a CoordinatorLayout's gravity.
         */
        fun fromGravity(gravity: Int): Orientation {
            return if ((gravity and Gravity.BOTTOM) == Gravity.BOTTOM) {
                UP
            } else {
                DOWN
            }
        }
    }
}
