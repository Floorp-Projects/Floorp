/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.CallSuper
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_browser.view.*
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.feature.app.links.AppLinksFeature
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.downloads.manager.FetchDownloadManager
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.CoordinateScrollingFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SwipeRefreshFeature
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.samples.browser.downloads.DownloadService
import org.mozilla.samples.browser.ext.components
import org.mozilla.samples.browser.integration.FindInPageIntegration

/**
 * Base fragment extended by [BrowserFragment] and [ExternalAppBrowserFragment].
 * This class only contains shared code focused on the main browsing content.
 * UI code specific to the app or to custom tabs can be found in the subclasses.
 */
abstract class BaseBrowserFragment : Fragment(), BackHandler {
    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val toolbarFeature = ViewBoundFeatureWrapper<ToolbarFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val promptFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val sitePermissionsFeature = ViewBoundFeatureWrapper<SitePermissionsFeature>()
    private val swipeRefreshFeature = ViewBoundFeatureWrapper<SwipeRefreshFeature>()
    private val appLinksFeature = ViewBoundFeatureWrapper<AppLinksFeature>()

    protected val sessionId: String?
        get() = arguments?.getString(SESSION_ID_KEY)

    @CallSuper
    @Suppress("LongMethod")
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val layout = inflater.inflate(R.layout.fragment_browser, container, false)

        layout.toolbar.setMenuBuilder(components.menuBuilder)

        sessionFeature.set(
            feature = SessionFeature(
                components.sessionManager,
                components.sessionUseCases.goBack,
                layout.engineView,
                sessionId),
            owner = this,
            view = layout)

        toolbarFeature.set(
            feature = ToolbarFeature(
                layout.toolbar,
                components.store,
                components.sessionUseCases.loadUrl,
                components.defaultSearchUseCase,
                sessionId),
            owner = this,
            view = layout)

        swipeRefreshFeature.set(
            feature = SwipeRefreshFeature(
                components.sessionManager,
                components.sessionUseCases.reload,
                layout.swipeToRefresh),
            owner = this,
            view = layout)

        val menuUpdaterFeature = object : SelectionAwareSessionObserver(components.sessionManager),
            LifecycleAwareFeature {
            override fun onLoadingStateChanged(session: Session, loading: Boolean) {
                layout.toolbar.invalidateActions()
            }

            override fun onNavigationStateChanged(session: Session, canGoBack: Boolean, canGoForward: Boolean) {
                layout.toolbar.invalidateActions()
            }

            override fun start() {
                observeIdOrSelected(sessionId)
            }
        }

        downloadsFeature.set(
            feature = DownloadsFeature(
                requireContext().applicationContext,
                store = components.store,
                useCases = components.downloadsUseCases,
                fragmentManager = childFragmentManager,
                onDownloadCompleted = { download, id ->
                    Logger.debug("Download done. ID#$id $download")
                },
                downloadManager = FetchDownloadManager(
                    requireContext().applicationContext,
                    DownloadService::class
                ),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                }),
            owner = this,
            view = layout)

        val scrollFeature = CoordinateScrollingFeature(components.sessionManager, layout.engineView, layout.toolbar)

        val contextMenuFeature = ContextMenuFeature(
            fragmentManager = requireFragmentManager(),
            store = components.store,
            candidates = ContextMenuCandidate.defaultCandidates(
                requireContext(),
                components.tabsUseCases,
                components.contextMenuUseCases,
                layout),
            engineView = layout.engineView,
            useCases = components.contextMenuUseCases)

        promptFeature.set(
            feature = PromptFeature(
                fragment = this,
                sessionManager = components.sessionManager,
                sessionId = sessionId,
                fragmentManager = requireFragmentManager(),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
                }),
            owner = this,
            view = layout)

        sitePermissionsFeature.set(
            feature = SitePermissionsFeature(
                context = requireContext(),
                sessionManager = components.sessionManager,
                sessionId = sessionId,
                fragmentManager = requireFragmentManager()
            ) { permissions ->
                requestPermissions(permissions, REQUEST_CODE_APP_PERMISSIONS)
            },
            owner = this,
            view = layout
        )

        findInPageIntegration.set(
            feature = FindInPageIntegration(components.store, layout.findInPage, layout.engineView),
            owner = this,
            view = layout)

        // Observe the lifecycle for supported features
        lifecycle.addObservers(
            scrollFeature,
            contextMenuFeature,
            menuUpdaterFeature
        )

        appLinksFeature.set(
            feature = AppLinksFeature(
                context = requireContext(),
                sessionManager = components.sessionManager,
                sessionId = sessionId,
                fragmentManager = requireFragmentManager(),
                interceptLinkClicks = true
            ),
            owner = this,
            view = layout
        )

        return layout
    }

    @CallSuper
    override fun onBackPressed(): Boolean =
        listOf(findInPageIntegration, toolbarFeature, sessionFeature).any { it.onBackPressed() }

    @CallSuper
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        val feature: PermissionsFeature? = when (requestCode) {
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.get()
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptFeature.get()
            REQUEST_CODE_APP_PERMISSIONS -> sitePermissionsFeature.get()
            else -> null
        }
        feature?.onPermissionsResult(permissions, grantResults)
    }

    @CallSuper
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptFeature.withFeature { it.onActivityResult(requestCode, resultCode, data) }
    }

    companion object {
        private const val SESSION_ID_KEY = "session_id"

        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val REQUEST_CODE_APP_PERMISSIONS = 3

        @JvmStatic
        protected fun Bundle.putSessionId(sessionId: String?) {
            putString(SESSION_ID_KEY, sessionId)
        }
    }
}
