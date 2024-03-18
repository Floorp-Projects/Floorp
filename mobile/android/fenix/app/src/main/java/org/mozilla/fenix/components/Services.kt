/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.accounts.FirefoxAccountsAuthFeature
import mozilla.components.feature.app.links.AppLinksInterceptor
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.perf.lazyMonitored
import org.mozilla.fenix.settings.SupportUtils

/**
 * Component group which encapsulates foreground-friendly services.
 */
class Services(
    private val context: Context,
    private val accountManager: FxaAccountManager,
) {
    val accountsAuthFeature by lazyMonitored {
        FirefoxAccountsAuthFeature(accountManager, FxaServer.REDIRECT_URL) { context, authUrl ->
            CoroutineScope(Dispatchers.Main).launch {
                val intent = SupportUtils.createAuthCustomTabIntent(context, authUrl)
                context.startActivity(intent)
            }
        }
    }

    val appLinksInterceptor by lazyMonitored {
        AppLinksInterceptor(
            context,
            interceptLinkClicks = true,
            launchInApp = { context.settings().shouldOpenLinksInApp() },
        )
    }

    val urlRequestInterceptor by lazyMonitored {
        UrlRequestInterceptor(
            isDeviceRamAboveThreshold = context.application.isDeviceRamAboveThreshold,
        )
    }
}
