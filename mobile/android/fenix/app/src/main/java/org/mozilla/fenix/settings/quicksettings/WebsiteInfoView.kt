/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.databinding.QuicksettingsWebsiteInfoBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView

/**
 * MVI View that knows to display a whether the current website uses a secure connection or not.
 *
 * Currently it does not support any user interaction.
 *
 * @param container [ViewGroup] in which this View will inflate itself.
 * @param icons Icons component for loading, caching and processing website icons.
 * @param interactor [WebSiteInfoInteractor] which will have delegated to all user interactions.
 */
class WebsiteInfoView(
    container: ViewGroup,
    private val icons: BrowserIcons = container.context.components.core.icons,
    val interactor: WebSiteInfoInteractor,
) {
    val binding = QuicksettingsWebsiteInfoBinding.inflate(
        LayoutInflater.from(container.context), container, true
    )

    /**
     * Allows changing what this View displays.
     *
     * @param state [WebsiteInfoState] to be rendered.
     */
    fun update(state: WebsiteInfoState) {
        icons.loadIntoView(binding.faviconImage, state.websiteUrl)
        bindUrl(state.websiteUrl)
        bindSecurityInfo(state.websiteSecurityUiValues)
    }

    private fun bindUrl(websiteUrl: String) {
        binding.url.text = websiteUrl.tryGetHostFromUrl()
    }

    private fun bindSecurityInfo(uiValues: WebsiteSecurityUiValues) {
        binding.securityInfo.setText(uiValues.securityInfoRes)
        bindConnectionDetailsListener()
        binding.securityInfoIcon.setImageResource(uiValues.iconRes)
    }

    @VisibleForTesting
    internal fun bindConnectionDetailsListener() {
        binding.securityInfo.setOnClickListener {
            interactor.onConnectionDetailsClicked()
        }
    }

    @VisibleForTesting
    internal fun provideContext(): Context = binding.root.context
}
