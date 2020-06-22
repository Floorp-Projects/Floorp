/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser

import android.view.View
import android.widget.CheckBox
import android.widget.CompoundButton
import mozilla.components.browser.state.selector.findTabOrCustomTab

import org.mozilla.focus.R
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.UrlUtils

import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents

internal class RequestDesktopCheckItemViewHolder/* package */(
    itemView: View,
    private val fragment: BrowserFragment
) : BrowserMenuViewHolder(itemView), CompoundButton.OnCheckedChangeListener {
    private val checkbox: CheckBox = itemView.findViewById(R.id.check_menu_item_checkbox)

    init {
        val tab = itemView.context.components.store.state.findTabOrCustomTab(fragment.session.id)
        checkbox.isChecked = tab?.content?.desktopMode ?: false
        checkbox.setOnCheckedChangeListener(this)
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        fragment.setShouldRequestDesktop(isChecked)
        TelemetryWrapper.desktopRequestCheckEvent(isChecked)

        // Delay closing the menu and reloading the website a bit so that the user can actually see
        // the switch change its state.
        ThreadUtils.postToMainThreadDelayed(Runnable {
            menu.dismiss()

            val url = UrlUtils.stripSchemeAndSubDomain(fragment.session.url)
            fragment.requireComponents.sessionUseCases.loadUrl(url)
        }, ANIMATION_DURATION)
    }

    companion object {
        const val LAYOUT_ID = R.layout.request_desktop_check_menu_item

        /**
         * Switch.THUMB_ANIMATION_DURATION
         */
        const val ANIMATION_DURATION = 250L
    }
}
