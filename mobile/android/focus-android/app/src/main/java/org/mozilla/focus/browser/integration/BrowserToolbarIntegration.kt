/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import androidx.core.content.ContextCompat
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.toolbar.ToolbarPresenter
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.focus.R
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.utils.HardwareUtils

class BrowserToolbarIntegration(
    private val store: BrowserStore,
    toolbar: BrowserToolbar,
    fragment: BrowserFragment,
    private val customTabId: String? = null,
    private val sessionUseCases: SessionUseCases,
    customTabsUseCases: CustomTabsUseCases
) : LifecycleAwareFeature {
    private val presenter = ToolbarPresenter(
        toolbar,
        store,
        customTabId
    )

    private var customTabsFeature: CustomTabsToolbarFeature? = null
    private var navigationButtonsIntegration: NavigationButtonsIntegration? = null

    init {
        val context = toolbar.context

        toolbar.display.colors = toolbar.display.colors.copy(
            hint = ContextCompat.getColor(context, R.color.urlBarHintText),
            text = 0xFFFFFFFF.toInt()
        )

        toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.TRACKING_PROTECTION
        )

        toolbar.display.displayIndicatorSeparator = false

        toolbar.display.setOnSiteSecurityClickedListener {
            fragment.showSecurityPopUp()
        }

        if (customTabId != null) {
            customTabsFeature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = customTabId,
                useCases = customTabsUseCases,
                closeListener = { fragment.closeCustomTab() }
            )
        }

        if (HardwareUtils.isTablet(context)) {
            navigationButtonsIntegration = NavigationButtonsIntegration(
                context,
                store,
                toolbar,
                sessionUseCases,
                customTabId
            )
        }
    }

    override fun start() {
        presenter.start()

        customTabsFeature?.start()
        navigationButtonsIntegration?.start()
    }

    override fun stop() {
        presenter.stop()

        customTabsFeature?.stop()
        navigationButtonsIntegration?.stop()
    }
}
