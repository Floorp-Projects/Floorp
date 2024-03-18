/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import android.content.Context
import androidx.core.net.toUri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.trackingprotection.CookieBannerUIMode
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/**
 * Get the current status of cookie banner ui mode.
 */
suspend fun CookieBannersStorage.getCookieBannerUIMode(
    context: Context,
    tab: SessionState,
): CookieBannerUIMode {
    return if (context.settings().shouldUseCookieBannerPrivateMode) {
        val isSiteDomainReported = withContext(Dispatchers.IO) {
            val host = tab.content.url.toUri().host.orEmpty()
            val siteDomain = context.components.publicSuffixList.getPublicSuffixPlusOne(host).await()
            siteDomain?.let { isSiteDomainReported(it) }
        }

        if (isSiteDomainReported == true) {
            return CookieBannerUIMode.REQUEST_UNSUPPORTED_SITE_SUBMITTED
        }

        val hasException = withContext(Dispatchers.IO) {
            hasException(tab.content.url, tab.content.private)
        } ?: return CookieBannerUIMode.HIDE

        if (hasException) {
            CookieBannerUIMode.DISABLE
        } else {
            withContext(Dispatchers.Main) {
                tab.isCookieBannerSupported()
            }
        }
    } else {
        CookieBannerUIMode.HIDE
    }
}

private suspend fun SessionState.isCookieBannerSupported(): CookieBannerUIMode =
    suspendCoroutine { continuation ->
        engineState.engineSession?.hasCookieBannerRuleForSession(
            onResult = { isSupported ->
                val mode = if (isSupported) {
                    CookieBannerUIMode.ENABLE
                } else {
                    CookieBannerUIMode.SITE_NOT_SUPPORTED
                }
                continuation.resume(mode)
            },
            onException = {
                continuation.resume(CookieBannerUIMode.HIDE)
            },
        )
    }
