/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.feature.awesomebar.AwesomeBarFeature
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.media.fullscreen.MediaSessionFullscreenFeature
import mozilla.components.feature.search.SearchFeature
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.feature.tabs.toolbar.TabsToolbarFeature
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.WebExtensionToolbarFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.enterToImmersiveMode
import mozilla.components.support.ktx.android.view.exitImmersiveMode
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
    private val mediaSessionFullscreenFeature =
        ViewBoundFeatureWrapper<MediaSessionFullscreenFeature>()

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        super.onCreateView(inflater, container, savedInstanceState)
        val binding = super.binding
        ToolbarAutocompleteFeature(binding.toolbar, components.engine).apply {
            addHistoryStorageProvider(components.historyStorage)
            addDomainProvider(components.shippedDomainsProvider)
        }

        TabsToolbarFeature(
            toolbar = binding.toolbar,
            store = components.store,
            sessionId = sessionId,
            lifecycleOwner = viewLifecycleOwner,
            showTabs = ::showTabs,
            countBasedOnSelectedTabType = false,
        )

        AwesomeBarFeature(binding.awesomeBar, binding.toolbar, binding.engineView, components.icons)
            .addHistoryProvider(
                components.historyStorage,
                components.sessionUseCases.loadUrl,
                components.engine,
            )
            .addSessionProvider(
                resources,
                components.store,
                components.tabsUseCases.selectTab,
            )
            .addSearchActionProvider(
                components.store,
                searchUseCase = components.searchUseCases.defaultSearch,
            )
            .addSearchProvider(
                requireContext(),
                components.store,
                components.searchUseCases.defaultSearch,
                fetchClient = components.client,
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                engine = components.engine,
                filterExactMatch = true,
            )
            .addClipboardProvider(
                requireContext(),
                components.sessionUseCases.loadUrl,
                components.engine,
            )

        readerViewFeature.set(
            feature = ReaderViewIntegration(
                requireContext(),
                components.engine,
                components.store,
                binding.toolbar,
                binding.readerViewBar,
                binding.readerViewAppearanceButton,
            ),
            owner = this,
            view = binding.root,
        )

        fullScreenFeature.set(
            feature = FullScreenFeature(
                components.store,
                components.sessionUseCases,
                sessionId,
            ) { inFullScreen ->
                if (inFullScreen) {
                    activity?.enterToImmersiveMode()
                } else {
                    activity?.exitImmersiveMode()
                }
            },
            owner = this,
            view = binding.root,
        )

        mediaSessionFullscreenFeature.set(
            feature = MediaSessionFullscreenFeature(
                requireActivity(),
                components.store,
                sessionId,
            ),
            owner = this,
            view = binding.root,
        )

        thumbnailsFeature.set(
            feature = BrowserThumbnails(requireContext(), binding.engineView, components.store),
            owner = this,
            view = binding.root,
        )

        webExtToolbarFeature.set(
            feature = WebExtensionToolbarFeature(
                binding.toolbar,
                components.store,
            ),
            owner = this,
            view = binding.root,
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
            view = binding.root,
        )

        val windowFeature = WindowFeature(components.store, components.tabsUseCases)
        lifecycle.addObserver(windowFeature)

        return binding.root
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
