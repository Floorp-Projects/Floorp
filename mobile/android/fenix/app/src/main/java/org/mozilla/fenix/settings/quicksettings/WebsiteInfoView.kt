/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.quicksettings_website_info.view.*
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R

/**
 * MVI View that knows to display a whether the current website uses a secure connection or not.
 *
 * Currently it does not support any user interaction.
 *
 * @param containerView [ViewGroup] in which this View will inflate itself.
 */
class WebsiteInfoView(
    override val containerView: ViewGroup
) : LayoutContainer {
    val view: View = LayoutInflater.from(containerView.context)
        .inflate(R.layout.quicksettings_website_info, containerView, true)

    /**
     * Allows changing what this View displays.
     *
     * @param state [WebsiteInfoState] to be rendered.
     */
    fun update(state: WebsiteInfoState) {
        bindUrl(state.websiteUrl)
        bindSecurityInfo(state.securityInfoRes, state.iconRes, state.iconTintRes)
    }

    private fun bindUrl(url: String) {
        view.url.text = url
    }

    private fun bindSecurityInfo(
        @StringRes securityInfoRes: Int,
        @DrawableRes iconRes: Int,
        @ColorRes iconTintRes: Int
    ) {
        val icon = AppCompatResources.getDrawable(view.context, iconRes)
        icon?.setTint(ContextCompat.getColor(view.context, iconTintRes))
        view.securityInfo.setText(securityInfoRes)
        view.securityInfo.putCompoundDrawablesRelativeWithIntrinsicBounds(start = icon)
    }
}
