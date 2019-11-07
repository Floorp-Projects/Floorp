/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import android.view.Gravity
import android.view.View
import androidx.core.view.isGone
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.component_search.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.concept.engine.manifest.getOrNull
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.pwa.ext.getTrustedScope
import mozilla.components.feature.pwa.ext.trustedOrigins
import mozilla.components.feature.pwa.feature.ManifestUpdateFeature
import mozilla.components.feature.pwa.feature.WebAppActivityFeature
import mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature
import mozilla.components.feature.pwa.feature.WebAppSiteControlsFeature
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BaseBrowserFragment
import org.mozilla.fenix.browser.CustomTabContextMenuCandidate
import org.mozilla.fenix.browser.FenixSnackbarDelegate
import org.mozilla.fenix.components.toolbar.BrowserToolbarController
import org.mozilla.fenix.components.toolbar.BrowserToolbarInteractor
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents

/**
 * Fragment used for browsing the web within external apps.
 */
@ExperimentalCoroutinesApi
class ExternalAppBrowserFragment : BaseBrowserFragment(), BackHandler {

    private val args by navArgs<ExternalAppBrowserFragmentArgs>()

    private val customTabsIntegration = ViewBoundFeatureWrapper<CustomTabsIntegration>()
    private val hideToolbarFeature = ViewBoundFeatureWrapper<WebAppHideToolbarFeature>()

    @Suppress("LongMethod")
    override fun initializeUI(view: View): Session? {
        return super.initializeUI(view)?.also {
            val activity = requireActivity()
            val components = activity.components

            val manifest = args.webAppManifest?.let { json ->
                WebAppManifestParser().parse(json).getOrNull()
            }
            val trustedScopes = listOfNotNull(manifest?.getTrustedScope())

            customTabSessionId?.let { customTabSessionId ->
                customTabsIntegration.set(
                    feature = CustomTabsIntegration(
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

                hideToolbarFeature.set(
                    feature = WebAppHideToolbarFeature(
                        requireComponents.core.sessionManager,
                        toolbar,
                        customTabSessionId,
                        trustedScopes
                    ) { toolbarVisible ->
                        updateLayoutMargins(inFullScreen = !toolbarVisible)
                    },
                    owner = this,
                    view = toolbar
                )

                if (manifest != null) {
                    activity.lifecycle.addObservers(
                        WebAppActivityFeature(
                            activity,
                            components.core.icons,
                            manifest
                        ),
                        ManifestUpdateFeature(
                            activity.applicationContext,
                            requireComponents.core.sessionManager,
                            requireComponents.core.webAppShortcutManager,
                            requireComponents.core.webAppManifestStorage,
                            customTabSessionId,
                            manifest
                        )
                    )
                    viewLifecycleOwner.lifecycle.addObserver(
                        WebAppSiteControlsFeature(
                            activity.applicationContext,
                            requireComponents.core.sessionManager,
                            requireComponents.useCases.sessionUseCases.reload,
                            customTabSessionId,
                            manifest
                        )
                    )
                } else {
                    viewLifecycleOwner.lifecycle.addObserver(
                        PoweredByNotification(
                            activity.applicationContext,
                            requireComponents.core.store,
                            customTabSessionId
                        )
                    )
                }
            }

            consumeFrom(browserStore) {
                browserToolbarView.update(it)
            }

            consumeFrom(components.core.customTabsStore) { state ->
                getSessionById()
                    ?.let { session -> session.customTabConfig?.sessionToken }
                    ?.let { token -> state.tabs[token] }
                    ?.let { tabState ->
                        hideToolbarFeature.withFeature {
                            it.onTrustedScopesChange(tabState.trustedOrigins)
                        }
                    }
            }

            updateLayoutMargins(false)
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

    override fun navToTrackingProtectionPanel(session: Session) {
        val useCase = TrackingProtectionUseCases(
            sessionManager = requireComponents.core.sessionManager,
            engine = requireComponents.core.engine
        )
        useCase.containsException(session) { contains ->
            val isEnabled = session.trackerBlockingEnabled && !contains
            val directions =
                ExternalAppBrowserFragmentDirections
                    .actionExternalAppBrowserFragmentToTrackingProtectionPanelDialogFragment(
                        sessionId = session.id,
                        url = session.url,
                        trackingProtectionEnabled = isEnabled,
                        gravity = getAppropriateLayoutGravity()
                    )
            nav(R.id.externalAppBrowserFragment, directions)
        }
    }

    override fun getEngineMargins(): Pair<Int, Int> {
        val toolbarHidden = toolbar.isGone
        return if (toolbarHidden) {
            0 to 0
        } else {
            val toolbarSize = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height)
            toolbarSize to 0
        }
    }

    override fun getContextMenuCandidates(
        context: Context,
        view: View
    ): List<ContextMenuCandidate> = CustomTabContextMenuCandidate.defaultCandidates(
        context,
        context.components.useCases.contextMenuUseCases,
        view,
        FenixSnackbarDelegate(
            view,
            null
        )
    )

    override fun getAppropriateLayoutGravity() = Gravity.TOP
}
