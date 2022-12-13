/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerexception

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.CookieBanner
import org.mozilla.focus.cookiebanner.CookieBannerOption
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings

class CookieBannerExceptionMiddleware(
    private val ioScope: CoroutineScope,
    private val cookieBannersStorage: CookieBannersStorage,
    private val appContext: Context,
    val uri: String,
) :
    Middleware<CookieBannerExceptionState, CookieBannerExceptionAction> {

    override fun invoke(
        context: MiddlewareContext<CookieBannerExceptionState, CookieBannerExceptionAction>,
        next: (CookieBannerExceptionAction) -> Unit,
        action: CookieBannerExceptionAction,
    ) {
        when (action) {
            is CookieBannerExceptionAction.InitCookieBannerException -> {
                /**
                 * The initial CookieBannerExceptionState when the user enters first in the screen
                 */
                val shouldShowCookieBannerItem = shouldShowCookieBannerExceptionItem()
                context.store.dispatch(
                    CookieBannerExceptionAction.UpdateCookieBannerExceptionExceptionVisibility(
                        shouldShowCookieBannerItem = shouldShowCookieBannerItem,
                    ),
                )
                if (shouldShowCookieBannerExceptionItem()) {
                    ioScope.launch {
                        val hasException = cookieBannersStorage.hasException(uri, true)
                        context.store.dispatch(
                            CookieBannerExceptionAction.UpdateCookieBannerExceptionException(hasException),
                        )
                    }
                }
            }
            is CookieBannerExceptionAction.ToggleCookieBannerExceptionException -> {
                ioScope.launch {
                    if (action.isCookieBannerHandlingExceptionEnabled) {
                        cookieBannersStorage.removeException(uri, true)
                        CookieBanner.exceptionRemoved.record(NoExtras())
                    } else {
                        cookieBannersStorage.addException(uri, true)
                        CookieBanner.exceptionAdded.record(NoExtras())
                    }
                    context.store.dispatch(
                        CookieBannerExceptionAction.UpdateCookieBannerExceptionException(
                            !action.isCookieBannerHandlingExceptionEnabled,
                        ),
                    )
                    appContext.components.sessionUseCases.reload()
                }
                next(action)
            }
            else -> {
                next(action)
            }
        }
    }

    /**
     * It returns the cookie banner exception item visibility from tracking protection panel .
     * If the item is invisible item details should also be invisible.
     */
    private fun shouldShowCookieBannerExceptionItem(): Boolean {
        return appContext.settings.isCookieBannerEnable &&
            appContext.settings.getCurrentCookieBannerOptionFromSharePref() != CookieBannerOption.CookieBannerDisabled()
    }
}
