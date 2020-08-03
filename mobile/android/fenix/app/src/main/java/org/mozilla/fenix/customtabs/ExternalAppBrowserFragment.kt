/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import android.content.Intent
import android.os.SystemClock
import android.view.View
import androidx.core.view.isVisible
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.component_browser_top_toolbar.*
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.concept.engine.manifest.getOrNull
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.customtabs.CustomTabWindowFeature
import mozilla.components.feature.pwa.feature.ManifestUpdateFeature
import mozilla.components.feature.pwa.feature.WebAppActivityFeature
import mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature
import mozilla.components.feature.pwa.feature.WebAppSiteControlsFeature
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BaseBrowserFragment
import org.mozilla.fenix.browser.CustomTabContextMenuCandidate
import org.mozilla.fenix.browser.FenixSnackbarDelegate
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings

/**
 * Fragment used for browsing the web within external apps.
 */
@ExperimentalCoroutinesApi
class ExternalAppBrowserFragment : BaseBrowserFragment(), UserInteractionHandler {

    private val args by navArgs<ExternalAppBrowserFragmentArgs>()

    private val customTabsIntegration = ViewBoundFeatureWrapper<CustomTabsIntegration>()
    private val windowFeature = ViewBoundFeatureWrapper<CustomTabWindowFeature>()
    private val hideToolbarFeature = ViewBoundFeatureWrapper<WebAppHideToolbarFeature>()

    @Suppress("LongMethod", "ComplexMethod")
    override fun initializeUI(view: View): Session? {
        return super.initializeUI(view)?.also {
            val activity = requireActivity()
            val components = activity.components

            val manifest = args.webAppManifest?.let { json ->
                WebAppManifestParser().parse(json).getOrNull()
            }

            customTabSessionId?.let { customTabSessionId ->
                customTabsIntegration.set(
                    feature = CustomTabsIntegration(
                        sessionManager = requireComponents.core.sessionManager,
                        toolbar = toolbar,
                        sessionId = customTabSessionId,
                        activity = activity,
                        onItemTapped = { browserInteractor.onBrowserToolbarMenuItemTapped(it) },
                        isPrivate = it.private,
                        shouldReverseItems = !activity.settings().shouldUseBottomToolbar
                    ),
                    owner = this,
                    view = view
                )

                windowFeature.set(
                    feature = CustomTabWindowFeature(
                        activity,
                        components.core.store,
                        customTabSessionId
                    ) { uri ->
                        val intent = Intent.parseUri("${BuildConfig.DEEP_LINK_SCHEME}://open?url=$uri", 0)
                        if (intent.action == Intent.ACTION_VIEW) {
                            intent.addCategory(Intent.CATEGORY_BROWSABLE)
                            intent.component = null
                            intent.selector = null
                            intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                        }
                        activity.startActivity(intent)
                    },
                    owner = this,
                    view = view
                )

                hideToolbarFeature.set(
                    feature = WebAppHideToolbarFeature(
                        store = requireComponents.core.store,
                        customTabsStore = requireComponents.core.customTabsStore,
                        tabId = customTabSessionId,
                        manifest = manifest
                    ) { toolbarVisible ->
                        browserToolbarView.view.isVisible = toolbarVisible
                        webAppToolbarShouldBeVisible = toolbarVisible
                        if (!toolbarVisible) { engineView.setDynamicToolbarMaxHeight(0) }
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
                            manifest,
                            WebAppSiteControlsBuilder(
                                requireComponents.core.sessionManager,
                                requireComponents.useCases.sessionUseCases.reload,
                                customTabSessionId,
                                manifest
                            )
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
        }
    }

    override fun onResume() {
        super.onResume()
        val currTimeMs = SystemClock.elapsedRealtimeNanos() / MS_PRECISION
        requireComponents.analytics.metrics.track(
            Event.ProgressiveWebAppForeground(currTimeMs)
        )
    }

    override fun onPause() {
        super.onPause()
        val currTimeMs = SystemClock.elapsedRealtimeNanos() / MS_PRECISION
        requireComponents.analytics.metrics.track(
            Event.ProgressiveWebAppBackground(currTimeMs)
        )
    }

    override fun removeSessionIfNeeded(): Boolean {
        return customTabsIntegration.onBackPressed() || super.removeSessionIfNeeded()
    }

    override fun navToQuickSettingsSheet(session: Session, sitePermissions: SitePermissions?) {
        val directions = ExternalAppBrowserFragmentDirections
            .actionGlobalQuickSettingsSheetDialogFragment(
                sessionId = session.id,
                url = session.url,
                title = session.title,
                isSecured = session.securityInfo.secure,
                sitePermissions = sitePermissions,
                gravity = getAppropriateLayoutGravity(),
                certificateName = session.securityInfo.issuer
            )
        nav(R.id.externalAppBrowserFragment, directions)
    }

    override fun navToTrackingProtectionPanel(session: Session) {
        requireComponents.useCases.trackingProtectionUseCases.containsException(session.id) { contains ->
            val isEnabled = session.trackerBlockingEnabled && !contains
            val directions =
                ExternalAppBrowserFragmentDirections
                    .actionGlobalTrackingProtectionPanelDialogFragment(
                        sessionId = session.id,
                        url = session.url,
                        trackingProtectionEnabled = isEnabled,
                        gravity = getAppropriateLayoutGravity()
                    )
            nav(R.id.externalAppBrowserFragment, directions)
        }
    }

    override fun getContextMenuCandidates(
        context: Context,
        view: View
    ): List<ContextMenuCandidate> = CustomTabContextMenuCandidate.defaultCandidates(
        context,
        context.components.useCases.contextMenuUseCases,
        view,
        FenixSnackbarDelegate(view)
    )

    companion object {
        // We only care about millisecond precision for telemetry events
        internal const val MS_PRECISION = 1_000_000L
    }
}
