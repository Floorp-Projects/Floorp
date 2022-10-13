/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter.icons

import android.view.LayoutInflater
import android.widget.TextView
import androidx.annotation.LayoutRes
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet.BOTTOM
import androidx.constraintlayout.widget.ConstraintSet.END
import androidx.constraintlayout.widget.ConstraintSet.PARENT_ID
import androidx.constraintlayout.widget.ConstraintSet.START
import androidx.constraintlayout.widget.ConstraintSet.TOP
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.ext.applyStyle
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.TextMenuIcon

internal class TextMenuIconViewHolder(
    parent: ConstraintLayout,
    inflater: LayoutInflater,
    side: Side,
) : MenuIconViewHolder<TextMenuIcon>(parent, inflater) {

    private val textView: TextView = inflate(layoutResource).findViewById(R.id.icon)

    init {
        updateConstraints {
            connect(textView.id, TOP, PARENT_ID, TOP)
            connect(textView.id, BOTTOM, PARENT_ID, BOTTOM)
            when (side) {
                Side.START -> {
                    connect(textView.id, START, PARENT_ID, START)
                    connect(textView.id, END, R.id.label, START)
                    connect(R.id.label, START, textView.id, END)
                }
                Side.END -> {
                    connect(textView.id, END, PARENT_ID, END)
                    connect(textView.id, START, R.id.label, END)
                    connect(R.id.label, END, textView.id, START)
                }
            }
        }
    }

    override fun bind(newIcon: TextMenuIcon, oldIcon: TextMenuIcon?) {
        textView.text = newIcon.text
        textView.applyStyle(newIcon.textStyle, oldIcon?.textStyle)
    }

    override fun disconnect() {
        parent.removeView(textView)
        super.disconnect()
    }

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_icon_text
    }
}
