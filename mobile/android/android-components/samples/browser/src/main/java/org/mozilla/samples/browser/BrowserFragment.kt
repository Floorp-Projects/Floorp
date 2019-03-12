/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Intent
import android.os.Bundle
import android.support.v4.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_browser.view.*
import mozilla.components.feature.awesomebar.AwesomeBarFeature
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.CoordinateScrollingFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.ThumbnailsFeature
import mozilla.components.feature.session.WindowFeature
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.feature.tabs.toolbar.TabsToolbarFeature
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.samples.browser.ext.components
import org.mozilla.samples.browser.integration.FindInPageIntegration

class BrowserFragment : Fragment(), BackHandler {
    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val toolbarFeature = ViewBoundFeatureWrapper<ToolbarFeature>()
    private val customTabsToolbarFeature = ViewBoundFeatureWrapper<CustomTabsToolbarFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val promptFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val sitePermissionsFeature = ViewBoundFeatureWrapper<SitePermissionsFeature>()
    private val thumbnailsFeature = ViewBoundFeatureWrapper<ThumbnailsFeature>()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val layout = inflater.inflate(R.layout.fragment_browser, container, false)

        layout.toolbar.setMenuBuilder(components.menuBuilder)

        val sessionId = arguments?.getString(SESSION_ID)

        sessionFeature.set(
            feature = SessionFeature(
                components.sessionManager,
                components.sessionUseCases,
                layout.engineView,
                sessionId),
            owner = this,
            view = layout)

        toolbarFeature.set(
            feature = ToolbarFeature(
                layout.toolbar,
                components.sessionManager,
                components.sessionUseCases.loadUrl,
                components.defaultSearchUseCase,
                sessionId),
            owner = this,
            view = layout)

        ToolbarAutocompleteFeature(layout.toolbar).apply {
            addHistoryStorageProvider(components.historyStorage)
            addDomainProvider(components.shippedDomainsProvider)
        }

        TabsToolbarFeature(layout.toolbar, components.sessionManager, sessionId, ::showTabs)

        AwesomeBarFeature(layout.awesomeBar, layout.toolbar, layout.engineView, components.icons)
            .addHistoryProvider(
                components.historyStorage,
                components.sessionUseCases.loadUrl)
            .addSessionProvider(components.sessionManager, components.tabsUseCases.selectTab)
            .addSearchProvider(
                components.searchEngineManager.getDefaultSearchEngine(requireContext()),
                components.searchUseCases.defaultSearch,
                HttpURLConnectionClient())
            .addClipboardProvider(requireContext(), components.sessionUseCases.loadUrl)

        downloadsFeature.set(
            feature = DownloadsFeature(
                requireContext(),
                sessionManager = components.sessionManager,
                fragmentManager = childFragmentManager,
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                }),
            owner = this,
            view = layout)

        val scrollFeature = CoordinateScrollingFeature(components.sessionManager, layout.engineView, layout.toolbar)

        val contextMenuFeature = ContextMenuFeature(
            requireFragmentManager(),
            components.sessionManager,
            ContextMenuCandidate.defaultCandidates(
                requireContext(),
                components.tabsUseCases,
                layout),
            layout.engineView)

        promptFeature.set(
            feature = PromptFeature(
                fragment = this,
                sessionManager = components.sessionManager,
                fragmentManager = requireFragmentManager(),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
                }),
            owner = this,
            view = layout)

        val windowFeature = WindowFeature(components.engine, components.sessionManager)

        sitePermissionsFeature.set(
            feature = SitePermissionsFeature(
                anchorView = layout.toolbar,
                sessionManager = components.sessionManager
            ) { permissions ->
                requestPermissions(permissions, REQUEST_CODE_APP_PERMISSIONS)
            },
            owner = this,
            view = layout
        )

        customTabsToolbarFeature.set(
            feature = CustomTabsToolbarFeature(
                components.sessionManager,
                layout.toolbar,
                sessionId,
                components.menuBuilder,
                closeListener = { activity?.finish() }),
            owner = this,
            view = layout)

        findInPageIntegration.set(
            feature = FindInPageIntegration(components.sessionManager, layout.findInPage),
            owner = this,
            view = layout)

        // Observe the lifecycle for supported features
        lifecycle.addObservers(
            scrollFeature,
            contextMenuFeature,
            windowFeature
        )

        thumbnailsFeature.set(
            feature = ThumbnailsFeature(requireContext(), layout.engineView, components.sessionManager),
            owner = this,
            view = layout
        )

        return layout
    }

    private fun showTabs() {
        // For now we are performing manual fragment transactions here. Once we can use the new
        // navigation support library we may want to pass navigation graphs around.
        activity?.supportFragmentManager?.beginTransaction()?.apply {
            replace(R.id.container, TabsTrayFragment())
            commit()
        }
    }

    override fun onBackPressed(): Boolean {
        return when {
            findInPageIntegration.onBackPressed() -> true
            toolbarFeature.onBackPressed() -> true
            sessionFeature.onBackPressed() -> true
            customTabsToolbarFeature.onBackPressed() -> true
            else -> false
        }
    }

    companion object {
        private const val SESSION_ID = "session_id"
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val REQUEST_CODE_APP_PERMISSIONS = 3

        fun create(sessionId: String? = null): BrowserFragment = BrowserFragment().apply {
            arguments = Bundle().apply {
                putString(SESSION_ID, sessionId)
            }
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
            }
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
            }
            REQUEST_CODE_APP_PERMISSIONS -> sitePermissionsFeature.withFeature {
                it.onPermissionsResult(grantResults)
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptFeature.withFeature { onActivityResult(requestCode, resultCode, data) }
    }
}
