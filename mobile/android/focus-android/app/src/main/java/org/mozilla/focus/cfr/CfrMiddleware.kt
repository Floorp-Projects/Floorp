/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.cfr

import android.content.Context
import androidx.core.net.toUri
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CookieBannerAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.CookieBanner
import org.mozilla.focus.cookiebanner.CookieBannerOption
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ext.truncatedHost
import org.mozilla.focus.nimbus.FocusNimbus
import org.mozilla.focus.nimbus.Onboarding
import org.mozilla.focus.state.AppAction

/**
 * Middleware used to intercept browser store actions in order to decide when should we display a specific CFR
 */
class CfrMiddleware(private val appContext: Context) : Middleware<BrowserState, BrowserAction> {
    private val onboardingFeature = FocusNimbus.features.onboarding
    private lateinit var onboardingConfig: Onboarding
    private val components = appContext.components
    private var isCurrentTabSecure = false
    private var tpExposureAlreadyRecorded = false

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        onboardingConfig = onboardingFeature.value()
        if (onboardingConfig.isCfrEnabled) {
            next(action)
            showCookieBannerCfr(action)
            showTrackingProtectionCfr(action, context)
        } else {
            next(action)
        }
    }

    private fun showCookieBannerCfr(
        action: BrowserAction,
    ) {
        if (action is CookieBannerAction.UpdateStatusAction &&
            shouldShowCookieBannerCfr(action) &&
            otherCfrHasBeenShown()
        ) {
            CookieBanner.cookieBannerCfrShown.record(NoExtras())
            components.appStore.dispatch(
                AppAction.ShowCookieBannerCfrChange(true),
            )
        }
    }

    private fun showTrackingProtectionCfr(
        action: BrowserAction,
        context: MiddlewareContext<BrowserState, BrowserAction>,
    ) {
        if (action is ContentAction.UpdateSecurityInfoAction) {
            isCurrentTabSecure = action.securityInfo.secure
        }
        if (shouldShowCfrForTrackingProtection(action = action, browserState = context.state)) {
            if (!tpExposureAlreadyRecorded) {
                FocusNimbus.features.onboarding.recordExposure()
                tpExposureAlreadyRecorded = true
            }

            components.appStore.dispatch(
                AppAction.ShowTrackingProtectionCfrChange(
                    mapOf((action as TrackingProtectionAction.TrackerBlockedAction).tabId to true),
                ),
            )
        }
    }

    private fun isMozillaUrl(browserState: BrowserState): Boolean {
        return browserState.findTabOrCustomTabOrSelectedTab(
            browserState.selectedTabId,
        )?.content?.url?.toUri()?.truncatedHost()?.substringBefore(".") == ("mozilla")
    }

    private fun isActionSecure(action: BrowserAction) =
        action is TrackingProtectionAction.TrackerBlockedAction && isCurrentTabSecure

    private fun shouldShowCfrForTrackingProtection(
        action: BrowserAction,
        browserState: BrowserState,
    ) = (
        isActionSecure(action = action) &&
            !isMozillaUrl(browserState = browserState) &&
            components.settings.shouldShowCfrForTrackingProtection &&
            !components.appStore.state.showEraseTabsCfr
        )

    private fun otherCfrHasBeenShown(): Boolean {
        return (
            !appContext.settings.shouldShowCfrForTrackingProtection &&
                !components.appStore.state.showEraseTabsCfr
            )
    }

    private fun shouldShowCookieBannerCfr(action: CookieBannerAction.UpdateStatusAction): Boolean {
        return (
            !appContext.settings.isFirstRun &&
                appContext.settings.shouldShowCookieBannerCfr &&
                appContext.settings.isCookieBannerEnable &&
                appContext.settings.getCurrentCookieBannerOptionFromSharePref() ==
                CookieBannerOption.CookieBannerRejectAll() &&
                action.status == EngineSession.CookieBannerHandlingStatus.HANDLED
            )
    }
}
