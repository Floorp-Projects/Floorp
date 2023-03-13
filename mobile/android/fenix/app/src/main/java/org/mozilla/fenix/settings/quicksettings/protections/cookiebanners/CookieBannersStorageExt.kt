/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
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
    return if (context.settings().shouldUseCookieBanner) {
        val hasException = withContext(Dispatchers.IO) {
            hasException(tab.content.url, tab.content.private)
        }
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
