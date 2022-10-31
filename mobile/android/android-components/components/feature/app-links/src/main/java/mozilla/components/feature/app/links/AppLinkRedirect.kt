/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Intent

/**
 * Data class for the external Intent or fallback URL a given URL encodes for.
 */
data class AppLinkRedirect(
    val appIntent: Intent?,
    val fallbackUrl: String?,
    val marketplaceIntent: Intent?,
) {
    /**
     * If there is a third-party app intent.
     */
    fun hasExternalApp() = appIntent != null

    /**
     * If there is a fallback URL (should the intent fails).
     */
    fun hasFallback() = fallbackUrl != null

    /**
     * If there is a marketplace intent (should the external app is not installed).
     */
    fun hasMarketplaceIntent() = marketplaceIntent != null

    /**
     * If the app link is a redirect (to an app or URL).
     */
    fun isRedirect() = hasExternalApp() || hasFallback() || hasMarketplaceIntent()

    /**
     * Is the app link one that can be installed from a store.
     */
    fun isInstallable() = appIntent?.data?.scheme == "market"
}
