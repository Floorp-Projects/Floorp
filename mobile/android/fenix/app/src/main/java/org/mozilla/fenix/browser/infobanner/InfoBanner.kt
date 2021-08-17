/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.infobanner

import android.annotation.SuppressLint
import android.content.Context
import android.view.LayoutInflater
import android.view.View.GONE
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import org.mozilla.fenix.databinding.InfoBannerBinding
import org.mozilla.fenix.ext.settings

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
@SuppressWarnings("LongParameterList")
open class InfoBanner(
    private val context: Context,
    private val container: ViewGroup,
    private val message: String,
    private val dismissText: String,
    private val actionText: String? = null,
    private val dismissByHiding: Boolean = false,
    @VisibleForTesting
    internal val dismissAction: (() -> Unit)? = null,
    @VisibleForTesting
    internal val actionToPerform: (() -> Unit)? = null
) {
    @SuppressLint("InflateParams")
    @VisibleForTesting
    internal val binding = InfoBannerBinding.inflate(LayoutInflater.from(context), container, false)

    internal open fun showBanner() {
        binding.bannerInfoMessage.text = message
        binding.dismiss.text = dismissText

        if (actionText.isNullOrEmpty()) {
            binding.action.visibility = GONE
        } else {
            binding.action.text = actionText
        }

        container.addView(binding.root)

        binding.dismiss.setOnClickListener {
            dismissAction?.invoke()
            if (dismissByHiding) { binding.root.visibility = GONE } else { dismiss() }
        }

        binding.action.setOnClickListener {
            actionToPerform?.invoke()
        }

        context.settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
    }

    internal fun dismiss() {
        container.removeView(binding.root)
    }
}
