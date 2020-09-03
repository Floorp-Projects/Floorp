/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.ViewGroup
import androidx.navigation.NavController
import mozilla.components.browser.session.Session
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.Settings

/**
 * Displays an [InfoBanner] when a user visits a website that can be opened in an installed native app.
 */
class OpenInAppOnboardingObserver(
    private val context: Context,
    private val navController: NavController,
    private val settings: Settings,
    private val appLinksUseCases: AppLinksUseCases,
    private val container: ViewGroup
) : Session.Observer {

    private var sessionDomainForDisplayedBanner: String? = null
    private var infoBanner: InfoBanner? = null

    override fun onUrlChanged(session: Session, url: String) {
        sessionDomainForDisplayedBanner?.let {
            if (url.tryGetHostFromUrl() != it) {
                infoBanner?.dismiss()
            }
        }
    }

    override fun onLoadingStateChanged(session: Session, loading: Boolean) {
        val appLink = appLinksUseCases.appLinkRedirect

        if (!loading &&
            settings.shouldShowOpenInAppBanner &&
            appLink(session.url).hasExternalApp()
        ) {
            infoBanner = InfoBanner(
                context = context,
                message = context.getString(R.string.open_in_app_cfr_info_message),
                dismissText = context.getString(R.string.open_in_app_cfr_negative_button_text),
                actionText = context.getString(R.string.open_in_app_cfr_positive_button_text),
                container = container
            ) {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment(
                    preferenceToScrollTo = context.getString(R.string.pref_key_open_links_in_external_app)
                )
                navController.nav(R.id.browserFragment, directions)
            }

            infoBanner?.showBanner()
            sessionDomainForDisplayedBanner = session.url.tryGetHostFromUrl()
            settings.shouldShowOpenInAppBanner = false
        }
    }
}
