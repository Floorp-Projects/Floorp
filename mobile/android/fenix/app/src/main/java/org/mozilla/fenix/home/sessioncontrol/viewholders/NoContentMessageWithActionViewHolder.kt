/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.no_content_message_with_action.view.*
import org.mozilla.fenix.R

class NoContentMessageWithActionViewHolder(
    private val view: View
) : NoContentMessageViewHolder(view) {

    /**
     * @param icon The visible label for header text this menu item.
     * @param header ID of string resource for title text.
     * @param description ID of string resource for description text.
     * @param buttonIcon Optional ID of drawable resource for button icon.
     * @param buttonText Optional ID of string resource for button text.
     * @param listener Optional Callback to be invoked when the button is clicked.
     */
    @Suppress("LongParameterList")
    fun bind(
        @DrawableRes icon: Int,
        @StringRes header: Int,
        @StringRes description: Int,
        @DrawableRes buttonIcon: Int = 0,
        @StringRes buttonText: Int = 0,
        listener: (() -> Unit)? = null
    ) {
        super.bind(icon, header, description)
        with(view.context) {

            if (buttonIcon != 0 || buttonText != 0) {
                view.button_layout.isVisible = true
                view.button_icon.setImageDrawable(getDrawable(buttonIcon))
                view.button_text.text = getString(buttonText)
                view.button_layout.setOnClickListener {
                    listener?.invoke()
                }
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.no_content_message_with_action
    }
}
