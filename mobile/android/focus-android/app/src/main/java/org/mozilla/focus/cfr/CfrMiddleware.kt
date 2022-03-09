/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.cfr

import androidx.core.net.toUri
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.focus.Components
import org.mozilla.focus.ext.truncatedHost
import org.mozilla.focus.nimbus.FocusNimbus
import org.mozilla.focus.nimbus.Onboarding
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.ERASE_CFR_LIMIT

/**
 * Middleware used to intercept browser store actions in order to decide when should we display a specific CFR
 */
class CfrMiddleware(private val components: Components) : Middleware<BrowserState, BrowserAction> {
    private val onboardingFeature = FocusNimbus.features.onboarding
    private lateinit var onboardingConfig: Onboarding

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        onboardingFeature.recordExposure()
        onboardingConfig = onboardingFeature.value()
        next(action)

        if (action is TabListAction.AddTabAction &&
            onboardingConfig.isCfrEnabled &&
            !components.appStore.state.showTrackingProtectionCfr
        ) {
            components.settings.numberOfTabsOpened++
            if (components.settings.numberOfTabsOpened == ERASE_CFR_LIMIT) {
                components.appStore.dispatch(AppAction.ShowEraseTabsCfrChange(true))
            }
        }

        if (shouldShowCfrForTrackingProtection(action = action, browserState = context.state)) {
            components.appStore.dispatch(AppAction.ShowTrackingProtectionCfrChange(true))
        }
    }

    private fun isMozillaUrl(browserState: BrowserState): Boolean {
        return browserState.findTabOrCustomTabOrSelectedTab(
            browserState.selectedTabId
        )?.content?.url?.toUri()?.truncatedHost()?.substringBefore(".") == ("mozilla")
    }

    private fun isActionSecure(action: BrowserAction) = (
        action is ContentAction.UpdateSecurityInfoAction &&
            action.securityInfo.secure
        )

    private fun shouldShowCfrForTrackingProtection(
        action: BrowserAction,
        browserState: BrowserState
    ) = (
        isActionSecure(action = action) &&
            !isMozillaUrl(browserState = browserState) &&
            onboardingConfig.isCfrEnabled &&
            components.settings.shouldShowCfrForTrackingProtection &&
            !components.appStore.state.showEraseTabsCfr
        )
}
