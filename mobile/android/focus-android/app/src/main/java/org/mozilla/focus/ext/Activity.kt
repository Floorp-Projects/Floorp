/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.app.Activity
import androidx.annotation.DrawableRes
import androidx.appcompat.app.AppCompatActivity

/**
 * Sets the icon for the back (up) navigation button.
 * @param icon The resource id of the icon.
 */
fun Activity.setNavigationIcon(
    @DrawableRes icon: Int
) {
    (this as? AppCompatActivity)?.supportActionBar?.let {
        it.setDisplayHomeAsUpEnabled(true)
        it.setHomeAsUpIndicator(icon)
    }
}
