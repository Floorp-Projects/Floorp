/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_browser.view.*
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.feature.awesomebar.AwesomeBarFeature
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.media.fullscreen.MediaFullscreenOrientationFeature
import mozilla.components.feature.search.SearchFeature
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.feature.tabs.toolbar.TabsToolbarFeature
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.WebExtensionToolbarFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.enterToImmersiveMode
import mozilla.components.support.ktx.android.view.exitImmersiveModeIfNeeded
import org.mozilla.samples.browser.ext.components
import org.mozilla.samples.browser.integration.ReaderViewIntegration

/**
 * Fragment used for browsing the web within the main app.
 */
class BrowserFragment : BaseBrowserFragment(), UserInteractionHandler {
    private val thumbnailsFeature = ViewBoundFeatureWrapper<BrowserThumbnails>()
    private val readerViewFeature = ViewBoundFeatureWrapper<ReaderViewIntegration>()
    private val webExtToolbarFeature = ViewBoundFeatureWrapper<WebExtensionToolbarFeature>()
    private val searchFeature = ViewBoundFeatureWrapper<SearchFeature>()
    private val fullScreenFeature = ViewBoundFeatureWrapper<FullScreenFeature>()
    private val fullScreenMediaFeature =
        ViewBoundFeatureWrapper<MediaFullscreenOrientationFeature>()

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val layout = super.onCreateView(inflater, container, savedInstanceState)

        ToolbarAutocompleteFeature(layout.toolbar, components.engine).apply {
            addHistoryStorageProvider(components.historyStorage)
            addDomainProvider(components.shippedDomainsProvider)
        }

        TabsToolbarFeature(layout.toolbar, components.sessionManager, sessionId, ::showTabs)

        val applicationContext = requireContext().applicationContext

        AwesomeBarFeature(layout.awesomeBar, layout.toolbar, layout.engineView, components.icons)
            .addHistoryProvider(
                components.historyStorage,
                components.sessionUseCases.loadUrl,
                components.engine
            )
            .addSessionProvider(
                resources,
                components.store,
                components.tabsUseCases.selectTab
            )
            .addSearchActionProvider(
                searchEngineGetter = suspend {
                    components.searchEngineManager.getDefaultSearchEngine(applicationContext)
                },
                searchUseCase = components.searchUseCases.defaultSearch
            )
            .addSearchProvider(
                requireContext(),
                components.searchEngineManager,
                components.searchUseCases.defaultSearch,
                fetchClient = components.client,
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                engine = components.engine,
                filterExactMatch = true
            )
            .addClipboardProvider(
                requireContext(),
                components.sessionUseCases.loadUrl,
                components.engine
            )

        readerViewFeature.set(
            feature = ReaderViewIntegration(
                requireContext(),
                components.engine,
                components.store,
                layout.toolbar,
                layout.readerViewBar,
                layout.readerViewAppearanceButton
            ),
            owner = this,
            view = layout
        )

        fullScreenFeature.set(
            feature = FullScreenFeature(
                components.store,
                SessionUseCases(components.sessionManager),
                sessionId
            ) { inFullScreen ->
                if (inFullScreen) {
                    activity?.enterToImmersiveMode()
                } else {
                    activity?.exitImmersiveModeIfNeeded()
                }
            },
            owner = this,
            view = layout
        )

        fullScreenMediaFeature.set(
            feature = MediaFullscreenOrientationFeature(
                activity!!,
                components.store
            ),
            owner = this,
            view = layout
        )

        thumbnailsFeature.set(
            feature = BrowserThumbnails(requireContext(), layout.engineView, components.store),
            owner = this,
            view = layout
        )

        webExtToolbarFeature.set(
            feature = WebExtensionToolbarFeature(
                layout.toolbar,
                components.store
            ),
            owner = this,
            view = layout
        )

        searchFeature.set(
            feature = SearchFeature(components.store) { request, _ ->
                if (request.isPrivate) {
                    components.searchUseCases.newPrivateTabSearch.invoke(request.query)
                } else {
                    components.searchUseCases.newTabSearch.invoke(request.query)
                }
            },
            owner = this,
            view = layout
        )

        val windowFeature = WindowFeature(components.store, components.tabsUseCases)
        lifecycle.addObserver(windowFeature)

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
            fullScreenFeature.onBackPressed() -> true
            readerViewFeature.onBackPressed() -> true
            else -> super.onBackPressed()
        }
    }

    companion object {
        fun create(sessionId: String? = null) = BrowserFragment().apply {
            arguments = Bundle().apply {
                putSessionId(sessionId)
            }
        }
    }
}
