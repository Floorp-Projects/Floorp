/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.support.ktx.android.content.getDrawableWithTint
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.databinding.QuicksettingsWebsiteInfoBinding

/**
 * MVI View that knows to display a whether the current website uses a secure connection or not.
 *
 * Currently it does not support any user interaction.
 *
 * @param container [ViewGroup] in which this View will inflate itself.
 * @param icons Icons component for loading, caching and processing website icons.
 * @param interactor [WebSiteInfoInteractor] which will have delegated to all user interactions.
 * @param isDetailsMode Indicates if the view should be shown in detailed mode or not.
 * In normal mode only the url and connection status are visible.
 * In detailed mode, the title, certificate and back button are visible,
 * additionally to all the views in normal mode.
 */
class WebsiteInfoView(
    container: ViewGroup,
    private val icons: BrowserIcons = container.context.components.core.icons,
    val interactor: WebSiteInfoInteractor,
    val isDetailsMode: Boolean = false
) {

    val binding = QuicksettingsWebsiteInfoBinding.inflate(
        LayoutInflater.from(container.context),
        container,
        true
    )

    val layoutId =
        if (isDetailsMode) R.layout.connection_details_website_info else R.layout.quicksettings_website_info

    override val containerView: View = LayoutInflater.from(container.context)
        .inflate(layoutId, container, true)

    /**
     * Allows changing what this View displays.
     *
     * @param state [WebsiteInfoState] to be rendered.
     */
    fun update(state: WebsiteInfoState) {
        icons.loadIntoView(binding.favicon_image, state.websiteUrl)
        bindUrl(state.websiteUrl)
        bindSecurityInfo(state.websiteSecurityUiValues)
        if (isDetailsMode) {
            bindCertificateName(state.certificateName)
            bindTitle(state.websiteTitle)
            bindBackButtonListener()
        }
    }

    private fun bindUrl(websiteUrl: String) {
        url.text = if (isDetailsMode) websiteUrl else websiteUrl.tryGetHostFromUrl()
    }

    private fun bindSecurityInfo(uiValues: WebsiteSecurityUiValues) {
        val tint = getColor(containerView.context, uiValues.iconTintRes)
        securityInfo.setText(uiValues.securityInfoRes)
        if (!isDetailsMode) {
            bindConnectionDetailsListener()
        }
        securityInfoIcon.setImageDrawable(
            containerView.context.getDrawableWithTint(uiValues.iconRes, tint)
        )
    }

    @VisibleForTesting
    internal fun bindConnectionDetailsListener() {
        securityInfo.setOnClickListener {
            interactor.onConnectionDetailsClicked()
        }
    }

    @VisibleForTesting
    internal fun bindBackButtonListener() {
        details_back.isVisible = true
        details_back.setOnClickListener {
            interactor.onBackPressed()
        }
    }

    @VisibleForTesting
    internal fun bindTitle(websiteTitle: String) {
        title.text = websiteTitle
        if (websiteTitle.isEmpty()) {
            title_container.isVisible = false
        }
    }

    @VisibleForTesting
    internal fun bindCertificateName(cert: String) {
        val certificateLabel =
            containerView.context.getString(R.string.certificate_info_verified_by, cert)
        certificateInfo.text = certificateLabel
        certificateInfo.isVisible = cert.isNotEmpty()
    }
}
