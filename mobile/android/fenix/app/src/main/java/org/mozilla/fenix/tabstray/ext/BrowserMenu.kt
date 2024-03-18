/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import android.view.View
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import org.mozilla.fenix.R

/**
 * Invokes [BrowserMenu.show] and applies the default theme color background.
 */
fun BrowserMenu.showWithTheme(view: View) {
    show(view).also { popupMenu ->
        (popupMenu.contentView as? CardView)?.setCardBackgroundColor(
            ContextCompat.getColor(
                view.context,
                R.color.fx_mobile_layer_color_2,
            ),
        )
    }
}
