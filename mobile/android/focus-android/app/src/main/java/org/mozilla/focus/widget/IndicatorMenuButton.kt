/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import org.mozilla.focus.R
import org.mozilla.focus.whatsnew.WhatsNew

class IndicatorMenuButton(context: Context, attrs: AttributeSet) : FrameLayout(context, attrs) {
    init {
        isClickable = true

        val view = LayoutInflater.from(context).inflate(
            R.layout.item_indicator_menu_button,
            this,
            true
        )

        view.findViewById<View>(R.id.dot).visibility = if (WhatsNew.shouldHighlightWhatsNew(context))
            View.VISIBLE
        else
            View.GONE
    }
}
