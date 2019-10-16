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
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.quicksettings_website_info.view.*
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R

class WebsiteInfoView(
    override val containerView: ViewGroup
) : LayoutContainer {
    val view: View = LayoutInflater.from(containerView.context)
        .inflate(R.layout.quicksettings_website_info, containerView, true)

    fun update(state: WebsiteInfoState) {
        bindUrl(state.url)
        bindSecurityInfo(state.securityInfoRes, state.iconRes, state.iconTintRes)
    }

    private fun bindUrl(url: String) {
        view.url.text = url.toUri().hostWithoutCommonPrefixes
    }

    private fun bindSecurityInfo(
        @StringRes securityInfoRes: Int,
        @DrawableRes iconRes: Int,
        @ColorRes iconTintRes: Int
    ) {
        val icon = view.context.getDrawable(iconRes)
        icon?.setTint(ContextCompat.getColor(view.context, iconTintRes))
        view.securityInfo.setText(securityInfoRes)
        view.securityInfo.putCompoundDrawablesRelativeWithIntrinsicBounds(start = icon)
    }
}
