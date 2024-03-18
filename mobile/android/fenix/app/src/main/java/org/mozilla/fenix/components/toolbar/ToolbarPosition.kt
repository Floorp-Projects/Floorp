/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.Gravity

/**
 * Fenix lets the browser toolbar be placed at either the top or the bottom of the screen.
 * This enum represents the posible positions.
 *
 * @property androidGravity [Gravity] value corresponding to the position.
 * Used to position related elements such as a CFR tooltip.
 */
enum class ToolbarPosition(val androidGravity: Int) {
    BOTTOM(Gravity.BOTTOM),
    TOP(Gravity.TOP),
}
