/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.annotation.SuppressLint
import android.content.Context
import android.view.LayoutInflater
import android.view.View.GONE
import android.view.ViewGroup
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import kotlinx.android.synthetic.main.info_banner.view.*
import org.mozilla.fenix.R

/**
 * Displays an Info Banner in the specified container with a message and an optional action.
 * The container can be a placeholder layout inserted in the original screen, or an existing layout.
 *
 * @param context - A [Context] for accessing system resources.
 * @param container - The layout where the banner will be shown
 * @param message - The message displayed in the banner
 * @param dismissText - The text on the dismiss button
 * @param actionText - The text on the action to perform button
 * @param actionToPerform - The action to be performed on action button press
 */
class InfoBanner(
    private val context: Context,
    private val container: ViewGroup,
    private val message: String,
    private val dismissText: String,
    private val actionText: String? = null,
    private val actionToPerform: (() -> Unit)? = null
) {
    @SuppressLint("InflateParams")
    private val bannerLayout = LayoutInflater.from(context)
        .inflate(R.layout.info_banner, null)

    internal fun showBanner() {
        bannerLayout.banner_info_message.text = message
        bannerLayout.dismiss.text = dismissText

        if (actionText.isNullOrEmpty()) {
            bannerLayout.action.visibility = GONE
        } else {
            bannerLayout.action.text = actionText
        }

        container.addView(bannerLayout)

        val params = bannerLayout.layoutParams as ViewGroup.LayoutParams
        params.height = WRAP_CONTENT
        params.width = MATCH_PARENT

        bannerLayout.dismiss.setOnClickListener {
            dismiss()
        }

        bannerLayout.action.setOnClickListener {
            actionToPerform?.invoke()
        }
    }

    internal fun dismiss() {
        container.removeView(bannerLayout)
    }
}
