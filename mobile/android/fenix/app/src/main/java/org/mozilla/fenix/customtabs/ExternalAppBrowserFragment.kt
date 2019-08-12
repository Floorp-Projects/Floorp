/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.view.Gravity
import android.view.View
import kotlinx.android.synthetic.main.component_search.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BaseBrowserFragment
import org.mozilla.fenix.components.toolbar.BrowserToolbarController
import org.mozilla.fenix.components.toolbar.BrowserToolbarInteractor
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents

/**
 * Fragment used for browsing the web within external apps.
 */
@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
class ExternalAppBrowserFragment : BaseBrowserFragment(), BackHandler {

    private val customTabsIntegration = ViewBoundFeatureWrapper<CustomTabsIntegration>()

    override fun initializeUI(view: View): Session? {
        return super.initializeUI(view)?.also {

            customTabSessionId?.let { customTabSessionId ->
                customTabsIntegration.set(
                    feature = CustomTabsIntegration(
                        requireContext(),
                        requireComponents.core.sessionManager,
                        toolbar,
                        customTabSessionId,
                        activity,
                        view.nestedScrollQuickAction,
                        view.swipeRefresh,
                        onItemTapped = { browserInteractor.onBrowserToolbarMenuItemTapped(it) }
                    ),
                    owner = this,
                    view = view)
            }

            consumeFrom(browserStore) {
                browserToolbarView.update(it)
            }
        }
    }

    override fun removeSessionIfNeeded(): Boolean {
        return customTabsIntegration.onBackPressed() || super.removeSessionIfNeeded()
    }

    override fun createBrowserToolbarViewInteractor(
        browserToolbarController: BrowserToolbarController,
        session: Session?
    ) = BrowserToolbarInteractor(browserToolbarController)

    override fun navToQuickSettingsSheet(session: Session, sitePermissions: SitePermissions?) {
        val directions = ExternalAppBrowserFragmentDirections
            .actionExternalAppBrowserFragmentToQuickSettingsSheetDialogFragment(
                sessionId = session.id,
                url = session.url,
                isSecured = session.securityInfo.secure,
                isTrackingProtectionOn = session.trackerBlockingEnabled,
                sitePermissions = sitePermissions,
                gravity = getAppropriateLayoutGravity()
            )
        nav(R.id.externalAppBrowserFragment, directions)
    }

    override fun getEngineMargins(): Pair<Int, Int> {
        val toolbarSize = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height)
        return toolbarSize to 0
    }

    override fun getAppropriateLayoutGravity() = Gravity.TOP

    companion object {
        private const val SHARED_TRANSITION_MS = 200L
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
    }
}
