/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.widget.TextView
import androidx.annotation.DrawableRes
import androidx.appcompat.content.res.AppCompatResources
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.setBounds

/**
 * Sets the drawableStart of a header in an onboarding card.
 */
fun TextView.setOnboardingIcon(@DrawableRes id: Int) {
    val icon = AppCompatResources.getDrawable(context, id)
    val size = context.resources.getDimensionPixelSize(R.dimen.onboarding_header_icon_height_width)
    icon?.setBounds(size)

    setCompoundDrawablesRelative(icon, null, null, null)
}
