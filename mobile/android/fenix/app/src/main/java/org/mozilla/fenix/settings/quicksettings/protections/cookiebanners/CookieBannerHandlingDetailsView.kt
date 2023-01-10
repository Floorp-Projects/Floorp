/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.kotlin.toShortUrl
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentCookieBannerDetailsPanelBinding
import org.mozilla.fenix.trackingprotection.ProtectionsState

/**
 * MVI View that knows how to display cookie banner handling details for a site.
 *
 * @param container [ViewGroup] in which this View will inflate itself.
 * @param publicSuffixList To show short url.
 * @param interactor [CookieBannerDetailsInteractor] which will have delegated to all user interactions.
 */
class CookieBannerHandlingDetailsView(
    container: ViewGroup,
    private val context: Context,
    private val ioScope: CoroutineScope,
    private val publicSuffixList: PublicSuffixList,
    val interactor: CookieBannerDetailsInteractor,
) {
    val binding = ComponentCookieBannerDetailsPanelBinding.inflate(
        LayoutInflater.from(container.context),
        container,
        true,
    )

    /**
     * Allows changing what this View displays.
     */
    fun update(state: ProtectionsState) {
        bindTitle(state.url, state.isCookieBannerHandlingEnabled)
        bindBackButtonListener()
        bindDescription(state.isCookieBannerHandlingEnabled)
        bindSwitch(state.isCookieBannerHandlingEnabled)
    }

    @VisibleForTesting
    internal fun bindTitle(url: String, isCookieBannerHandlingEnabled: Boolean) {
        ioScope.launch {
            val host = url.toUri().host.orEmpty()
            val domain = publicSuffixList.getPublicSuffixPlusOne(host).await()

            launch(Dispatchers.Main) {
                val stringID =
                    if (isCookieBannerHandlingEnabled) {
                        R.string.reduce_cookie_banner_details_panel_title_off_for_site
                    } else {
                        R.string.reduce_cookie_banner_details_panel_title_on_for_site
                    }
                val data = domain ?: url
                val shortUrl = data.toShortUrl(publicSuffixList)
                binding.title.text = context.getString(stringID, shortUrl)
            }
        }
    }

    @VisibleForTesting
    internal fun bindDescription(isCookieBannerHandlingEnabled: Boolean) {
        val stringID =
            if (isCookieBannerHandlingEnabled) {
                R.string.reduce_cookie_banner_details_panel_description_off_for_site
            } else {
                R.string.reduce_cookie_banner_details_panel_description_on_for_site
            }
        val appName = context.getString(R.string.app_name)
        binding.details.text = context.getString(stringID, appName, appName)
    }

    @VisibleForTesting
    internal fun bindBackButtonListener() {
        binding.navigateBack.setOnClickListener {
            interactor.onBackPressed()
        }
    }

    @VisibleForTesting
    internal fun bindSwitch(isCookieBannerHandlingEnabled: Boolean) {
        binding.cookieBannerSwitch.isChecked = isCookieBannerHandlingEnabled
        binding.cookieBannerSwitch.setOnCheckedChangeListener { _, isChecked ->
            interactor.onTogglePressed(isChecked)
        }
    }
}
