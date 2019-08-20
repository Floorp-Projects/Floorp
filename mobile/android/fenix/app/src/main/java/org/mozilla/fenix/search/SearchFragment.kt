/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.Manifest
import android.content.Context
import android.content.DialogInterface
import android.graphics.Typeface.BOLD
import android.graphics.Typeface.ITALIC
import android.graphics.drawable.BitmapDrawable
import android.os.Bundle
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import androidx.transition.TransitionInflater
import kotlinx.android.synthetic.main.fragment_search.*
import kotlinx.android.synthetic.main.fragment_search.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.qr.QrFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.hasCamera
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.jetbrains.anko.backgroundDrawable
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.ext.getSpannable
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.toolbar.ToolbarView
import org.mozilla.fenix.utils.Settings

@Suppress("TooManyFunctions", "LargeClass")
class SearchFragment : Fragment(), BackHandler {
    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView
    private val qrFeature = ViewBoundFeatureWrapper<QrFeature>()
    private var permissionDidUpdate = false
    private lateinit var searchStore: SearchStore
    private lateinit var searchInteractor: SearchInteractor

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        postponeEnterTransition()
        sharedElementEnterTransition =
            TransitionInflater.from(context).inflateTransition(android.R.transition.move)
                .setDuration(
                    SHARED_TRANSITION_MS
                )
        requireComponents.analytics.metrics.track(Event.InteractWithSearchURLArea)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val session = arguments
            ?.let(SearchFragmentArgs.Companion::fromBundle)
            ?.let { it.sessionId }
            ?.let(requireComponents.core.sessionManager::findSessionById)

        val displayShortcutEnginePicker = arguments
            ?.let(SearchFragmentArgs.Companion::fromBundle)
            ?.let { it.showShortcutEnginePicker } ?: false

        val view = inflater.inflate(R.layout.fragment_search, container, false)
        val url = session?.url ?: ""
        val currentSearchEngine = SearchEngineSource.Default(
            requireComponents.search.searchEngineManager.getDefaultSearchEngine(requireContext())
        )

        searchStore = StoreProvider.get(this) {
            SearchStore(
                SearchState(
                    query = url,
                    showShortcutEnginePicker = displayShortcutEnginePicker,
                    searchEngineSource = currentSearchEngine,
                    defaultEngineSource = currentSearchEngine,
                    showSuggestions = Settings.getInstance(requireContext()).showSearchSuggestions,
                    showVisitedSitesBookmarks = Settings.getInstance(requireContext()).shouldShowVisitedSitesBookmarks,
                    session = session
                )
            )
        }

        val searchController = DefaultSearchController(
            activity as HomeActivity,
            searchStore,
            findNavController()
        )

        searchInteractor = SearchInteractor(
            searchController
        )

        awesomeBarView = AwesomeBarView(view.search_layout, searchInteractor)
        toolbarView = ToolbarView(
            view.toolbar_component_wrapper,
            searchInteractor,
            historyStorageProvider(),
            (activity as HomeActivity).browsingModeManager.mode.isPrivate
        )

        startPostponedEnterTransition()
        return view
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        searchScanButton.visibility = if (context?.hasCamera() == true) View.VISIBLE else View.GONE
        layoutComponents(view.search_layout)

        qrFeature.set(
            QrFeature(
                requireContext(),
                fragmentManager = requireFragmentManager(),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
                },
                onScanResult = { result ->
                    searchScanButton.isChecked = false
                    activity?.let {
                        AlertDialog.Builder(it).apply {
                            val spannable = resources.getSpannable(
                                R.string.qr_scanner_confirmation_dialog_message,
                                listOf(
                                    getString(R.string.app_name) to listOf(StyleSpan(BOLD)),
                                    result to listOf(StyleSpan(ITALIC))
                                )
                            )
                            setMessage(spannable)
                            setNegativeButton(R.string.qr_scanner_dialog_negative) { dialog: DialogInterface, _ ->
                                requireComponents.analytics.metrics.track(Event.QRScannerNavigationDenied)
                                dialog.cancel()
                            }
                            setPositiveButton(R.string.qr_scanner_dialog_positive) { dialog: DialogInterface, _ ->
                                requireComponents.analytics.metrics.track(Event.QRScannerNavigationAllowed)
                                (activity as HomeActivity)
                                    .openToBrowserAndLoad(
                                        searchTermOrURL = result,
                                        newTab = searchStore.state.session == null,
                                        from = BrowserDirection.FromSearch
                                    )
                                dialog.dismiss()
                            }
                            create()
                        }.show()
                        requireComponents.analytics.metrics.track(Event.QRScannerPromptDisplayed)
                    }
                }),
            owner = this,
            view = view
        )

        view.searchScanButton.setOnClickListener {
            toolbarView.view.clearFocus()
            requireComponents.analytics.metrics.track(Event.QRScannerOpened)
            qrFeature.get()?.scan(R.id.container)
        }

        view.toolbar_wrapper.clipToOutline = false

        searchShortcutsButton.setOnClickListener {
            val isOpen = searchStore.state.showShortcutEnginePicker
            searchStore.dispatch(SearchAction.ShowSearchShortcutEnginePicker(!isOpen))

            if (isOpen) {
                requireComponents.analytics.metrics.track(Event.SearchShortcutMenuClosed)
            } else {
                requireComponents.analytics.metrics.track(Event.SearchShortcutMenuOpened)
            }

            searchInteractor.turnOnStartedTyping()
        }

        consumeFrom(searchStore) {
            awesomeBarView.update(it)
            toolbarView.update(it)
            updateSearchEngineIcon(it)
            updateSearchShortuctsIcon(it)
            updateSearchWithLabel(it)
        }

        startPostponedEnterTransition()
    }

    override fun onResume() {
        super.onResume()

        // The user has the option to go to 'Shortcuts' -> 'Search engine settings' to modify the default search engine.
        // When returning from that settings screen we need to update it to account for any changes.
        val currentDefaultEngine =
            requireComponents.search.searchEngineManager.getDefaultSearchEngine(requireContext())
        if (searchStore.state.defaultEngineSource.searchEngine != currentDefaultEngine) {
            searchStore.dispatch(
                SearchAction.SelectNewDefaultSearchEngine
                    (currentDefaultEngine)
            )
        }

        if (!permissionDidUpdate) {
            toolbarView.view.requestFocus()
        }

        permissionDidUpdate = false
        (activity as AppCompatActivity).supportActionBar?.hide()
    }

    override fun onPause() {
        super.onPause()
        toolbarView.view.clearFocus()
    }

    override fun onBackPressed(): Boolean {
        return when {
            qrFeature.onBackPressed() -> {
                view?.searchScanButton?.isChecked = false
                toolbarView.view.requestFocus()
                true
            }
            else -> false
        }
    }

    private fun updateSearchEngineIcon(searchState: SearchState) {
        val searchIcon = searchState.searchEngineSource.searchEngine.icon
        val draw = BitmapDrawable(resources, searchIcon)
        val iconSize = resources.getDimension(R.dimen.preference_icon_drawable_size).toInt()
        draw.setBounds(0, 0, iconSize, iconSize)
        searchEngineIcon?.backgroundDrawable = draw
    }

    private fun updateSearchWithLabel(searchState: SearchState) {
        searchWithShortcuts.visibility =
            if (searchState.showShortcutEnginePicker) View.VISIBLE else View.GONE
    }

    private fun updateSearchShortuctsIcon(searchState: SearchState) {
        with(requireContext()) {
            val showShortcuts = searchState.showShortcutEnginePicker
            searchShortcutsButton?.isChecked = showShortcuts

            val color = if (showShortcuts) R.attr.contrastText else R.attr.primaryText

            searchShortcutsButton.compoundDrawables[0]?.setTint(getColorFromAttr(color))
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        when (requestCode) {
            REQUEST_CODE_CAMERA_PERMISSIONS -> qrFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)

                context?.let { context: Context ->
                    if (context.isPermissionGranted(Manifest.permission.CAMERA)) {
                        permissionDidUpdate = true
                    } else {
                        view?.searchScanButton?.isChecked = false
                    }
                }
            }
            else -> super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        }
    }

    private fun historyStorageProvider(): HistoryStorage? {
        return if (Settings.getInstance(requireContext()).shouldShowVisitedSitesBookmarks) {
            requireComponents.core.historyStorage
        } else null
    }

    companion object {
        private const val SHARED_TRANSITION_MS = 200L
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
    }
}
