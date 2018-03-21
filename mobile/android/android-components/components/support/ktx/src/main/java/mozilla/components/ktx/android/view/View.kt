/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ktx.android.view

import android.support.v4.view.ViewCompat
import android.view.View

/**
 * The resolved layout direction for this view.
 *
 * @return {@link #LAYOUT_DIRECTION_RTL} if the layout direction is RTL or returns
 * {@link #LAYOUT_DIRECTION_LTR} if the layout direction is not RTL.
 */
val View.layoutDirection: Int
    get() =
        ViewCompat.getLayoutDirection(this)

/**
 * Is the horizontal layout direction of this view from Right to Left?
 */
val View.isRTL: Boolean
    get() = layoutDirection == ViewCompat.LAYOUT_DIRECTION_RTL

/**
 * Is the horizontal layout direction of this view from Left to Right?
 */
val View.isLTR: Boolean
    get() = layoutDirection == ViewCompat.LAYOUT_DIRECTION_LTR
