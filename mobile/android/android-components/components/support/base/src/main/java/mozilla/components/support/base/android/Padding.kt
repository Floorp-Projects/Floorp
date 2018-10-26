/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

/**
 * A representation of an Android Padding.
 *
 * @param left Padding start in PX.
 * @param top Padding top in PX.
 * @param right Padding end in PX.
 * @param bottom Padding end in PX.
 */
data class Padding(val left: Int, val top: Int, val right: Int, val bottom: Int)
