/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerreducer

import android.content.Context
import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.CookieBanner
import org.mozilla.focus.GleanMetrics.Pings
import org.mozilla.focus.cookiebanner.CookieBannerOption
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings

/**
 * Middleware for cookie banner reduction.
 */
class CookieBannerReducerMiddleware(
    private val ioScope: CoroutineScope,
    private val cookieBannersStorage: CookieBannersStorage,
    private val appContext: Context,
    private val currentTab: SessionState,
) :
    Middleware<CookieBannerReducerState, CookieBannerReducerAction> {

    override fun invoke(
        context: MiddlewareContext<CookieBannerReducerState, CookieBannerReducerAction>,
        next: (CookieBannerReducerAction) -> Unit,
        action: CookieBannerReducerAction,
    ) {
        when (action) {
            is CookieBannerReducerAction.InitCookieBannerReducer -> {
                /**
                 * The initial CookieBannerReducerState when the user enters first in the screen
                 */
                initCookieBannerReducer(context)
            }

            is CookieBannerReducerAction.ToggleCookieBannerException -> {
                handleCookieBannerToggle(action, context)
                next(action)
            }
            is CookieBannerReducerAction.RequestReportSite -> {
                reportSite(action, context)
                next(action)
            }
            else -> {
                next(action)
            }
        }
    }

    private fun handleCookieBannerToggle(
        action: CookieBannerReducerAction.ToggleCookieBannerException,
        context: MiddlewareContext<CookieBannerReducerState, CookieBannerReducerAction>,
    ) {
        ioScope.launch {
            if (action.isCookieBannerHandlingExceptionEnabled) {
                cookieBannersStorage.removeException(currentTab.content.url, true)
                CookieBanner.exceptionRemoved.record(NoExtras())
                context.store.dispatch(
                    CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                        CookieBannerReducerStatus.NoException,
                    ),
                )
            } else {
                clearSiteData()
                cookieBannersStorage.addPersistentExceptionInPrivateMode(currentTab.content.url)
                CookieBanner.exceptionAdded.record(NoExtras())
                context.store.dispatch(
                    CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                        CookieBannerReducerStatus.HasException,
                    ),
                )
            }
            appContext.components.sessionUseCases.reload()
        }
    }

    private fun reportSite(
        action: CookieBannerReducerAction.RequestReportSite,
        context: MiddlewareContext<CookieBannerReducerState, CookieBannerReducerAction>,
    ) {
        CookieBanner.reportSiteDomain.set(action.siteToReport)
        Pings.cookieBannerReportSite.submit()
        context.store.dispatch(
            CookieBannerReducerAction.ShowSnackBarForSiteToReport(
                true,
            ),
        )
        context.store.dispatch(
            CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                CookieBannerReducerStatus.CookieBannerUnsupportedSiteRequestWasSubmitted,
            ),
        )
        ioScope.launch { cookieBannersStorage.saveSiteDomain(action.siteToReport) }
    }

    private fun initCookieBannerReducer(
        context: MiddlewareContext<CookieBannerReducerState, CookieBannerReducerAction>,
    ) {
        val shouldShowCookieBannerItem = shouldShowCookieBannerReducerItem()
        context.store.dispatch(
            CookieBannerReducerAction.UpdateCookieBannerReducerVisibility(
                shouldShowCookieBannerItem = shouldShowCookieBannerItem,
            ),
        )

        if (!shouldShowCookieBannerItem) {
            return
        }
        ioScope.launch {
            if (isSiteDomainReported(context)) {
                return@launch
            }
            val hasException =
                cookieBannersStorage.hasException(currentTab.content.url, true)
            withContext(Dispatchers.Main) {
                if (hasException == null) {
                    // An error occurred while querying the exception, let's hide the item.
                    context.store.dispatch(
                        CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                            null,
                        ),
                    )
                    return@withContext
                } else if (!hasException) {
                    showUnsupportedSiteIfNeeded(context)
                } else {
                    showExceptionStatus(context, true)
                }
            }
        }
    }

    private fun showUnsupportedSiteIfNeeded(
        context: MiddlewareContext<CookieBannerReducerState, CookieBannerReducerAction>,
    ) {
        currentTab.engineState.engineSession?.hasCookieBannerRuleForSession(
            onResult = { result ->
                if (result) {
                    showExceptionStatus(context, false)
                } else {
                    context.store.dispatch(
                        CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                            CookieBannerReducerStatus.CookieBannerSiteNotSupported,
                        ),
                    )
                }
            },
            onException = {
                context.store.dispatch(
                    CookieBannerReducerAction.UpdateCookieBannerReducerVisibility(
                        shouldShowCookieBannerItem = false,
                    ),
                )
            },
        )
    }

    private fun showExceptionStatus(
        context: MiddlewareContext<CookieBannerReducerState, CookieBannerReducerAction>,
        hasException: Boolean,
    ) {
        if (hasException) {
            context.store.dispatch(
                CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                    CookieBannerReducerStatus.HasException,
                ),
            )
        } else {
            context.store.dispatch(
                CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                    CookieBannerReducerStatus.NoException,
                ),
            )
        }
    }

    /**
     * It returns the cookie banner reducer item visibility from tracking protection panel .
     * If the item is invisible item details should also be invisible.
     */
    private fun shouldShowCookieBannerReducerItem(): Boolean {
        return appContext.settings.isCookieBannerEnable &&
            appContext.settings.getCurrentCookieBannerOptionFromSharePref() !=
            CookieBannerOption.CookieBannerDisabled()
    }

    private suspend fun isSiteDomainReported(
        context: MiddlewareContext<CookieBannerReducerState, CookieBannerReducerAction>,
    ): Boolean {
        val host = currentTab.content.url.toUri().host.orEmpty()
        val siteDomain =
            appContext.components.publicSuffixList.getPublicSuffixPlusOne(host).await()
        if (siteDomain != null && cookieBannersStorage.isSiteDomainReported(siteDomain)) {
            context.store.dispatch(
                CookieBannerReducerAction.UpdateCookieBannerReducerStatus(
                    CookieBannerReducerStatus.CookieBannerUnsupportedSiteRequestWasSubmitted,
                ),
            )
            return true
        }
        return false
    }

    private suspend fun clearSiteData() {
        val host = currentTab.content.url.toUri().host.orEmpty()
        val domain = appContext.components.publicSuffixList.getPublicSuffixPlusOne(host).await()
        withContext(Dispatchers.Main) {
            appContext.components.engine.clearData(
                host = domain,
                data = Engine.BrowsingData.select(
                    Engine.BrowsingData.AUTH_SESSIONS,
                    Engine.BrowsingData.ALL_SITE_DATA,
                ),
            )
        }
    }
}
