/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.Manifest
import android.app.Activity.RESULT_OK
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.graphics.Typeface.BOLD
import android.graphics.Typeface.ITALIC
import android.os.Bundle
import android.speech.RecognizerIntent
import android.speech.RecognizerIntent.EXTRA_RESULTS
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewStub
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.core.widget.NestedScrollView
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_search.*
import kotlinx.android.synthetic.main.fragment_search.view.*
import kotlinx.android.synthetic.main.search_suggestions_onboarding.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.qr.QrFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.hasCamera
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getSpannable
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.toolbar.ToolbarView
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.widget.VoiceSearchActivity.Companion.SPEECH_REQUEST_CODE

@Suppress("TooManyFunctions", "LargeClass")
class SearchFragment : Fragment(), UserInteractionHandler {
    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView
    private val qrFeature = ViewBoundFeatureWrapper<QrFeature>()
    private var permissionDidUpdate = false
    private lateinit var searchStore: SearchFragmentStore
    private lateinit var searchInteractor: SearchInteractor

    private val speechIntent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)

    private fun shouldShowSearchSuggestions(isPrivate: Boolean): Boolean =
        if (isPrivate) {
            requireContext().settings().shouldShowSearchSuggestions &&
                requireContext().settings().shouldShowSearchSuggestionsInPrivate
        } else {
            requireContext().settings().shouldShowSearchSuggestions
        }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val activity = activity as HomeActivity
        val args by navArgs<SearchFragmentArgs>()
        val session = args.sessionId
            ?.let(requireComponents.core.sessionManager::findSessionById)

        val view = inflater.inflate(R.layout.fragment_search, container, false)
        val url = session?.url.orEmpty()
        val currentSearchEngine = SearchEngineSource.Default(
            requireComponents.search.provider.getDefaultEngine(requireContext())
        )

        val isPrivate = activity.browsingModeManager.mode.isPrivate

        requireComponents.analytics.metrics.track(Event.InteractWithSearchURLArea)

        searchStore = StoreProvider.get(this) {
            SearchFragmentStore(
                SearchFragmentState(
                    query = url,
                    searchEngineSource = currentSearchEngine,
                    defaultEngineSource = currentSearchEngine,
                    showSearchSuggestions = shouldShowSearchSuggestions(isPrivate),
                    showSearchSuggestionsHint = false,
                    showSearchShortcuts = requireContext().settings().shouldShowSearchShortcuts && url.isEmpty(),
                    showClipboardSuggestions = requireContext().settings().shouldShowClipboardSuggestions,
                    showHistorySuggestions = requireContext().settings().shouldShowHistorySuggestions,
                    showBookmarkSuggestions = requireContext().settings().shouldShowBookmarkSuggestions,
                    session = session,
                    pastedText = args.pastedText,
                    searchAccessPoint = args.searchAccessPoint
                )
            )
        }

        val searchController = DefaultSearchController(
            activity = activity,
            store = searchStore,
            navController = findNavController(),
            viewLifecycleScope = viewLifecycleOwner.lifecycleScope,
            clearToolbarFocus = ::clearToolbarFocus
        )

        searchInteractor = SearchInteractor(
            searchController
        )

        awesomeBarView = AwesomeBarView(view.scrollable_area, searchInteractor)

        view.scrollView.setOnScrollChangeListener {
                _: NestedScrollView, _: Int, _: Int, _: Int, _: Int ->
            view.hideKeyboard()
        }

        toolbarView = ToolbarView(
            view.toolbar_component_wrapper,
            searchInteractor,
            historyStorageProvider(),
            isPrivate,
            requireComponents.core.engine
        )

        toolbarView.view.addEditAction(
            BrowserToolbar.Button(
                ContextCompat.getDrawable(requireContext(), R.drawable.ic_microphone)!!,
                requireContext().getString(R.string.voice_search_content_description),
                visible = { requireContext().settings().shouldShowVoiceSearch && speechIsAvailable() },
                listener = ::launchVoiceSearch
            )
        )

        val urlView = toolbarView.view
            .findViewById<InlineAutocompleteEditText>(R.id.mozac_browser_toolbar_edit_url_view)
        urlView?.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO

        requireComponents.core.engine.speculativeCreateSession(isPrivate)
        startPostponedEnterTransition()
        return view
    }

    private fun speechIsAvailable(): Boolean {
        return (speechIntent.resolveActivity(requireContext().packageManager) != null)
    }

    private fun launchVoiceSearch() {
        // Note if a user disables speech while the app is on the search fragment
        // the voice button will still be available and *will* cause a crash if tapped,
        // since the `visible` call is only checked on create. In order to avoid extra complexity
        // around such a small edge case, we make the button have no functionality in this case.
        if (!speechIsAvailable()) { return }

        requireComponents.analytics.metrics.track(Event.VoiceSearchTapped)
        speechIntent.apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_PROMPT, requireContext().getString(R.string.voice_search_explainer))
        }
        startActivityForResult(speechIntent, SPEECH_REQUEST_CODE)
    }

    private fun clearToolbarFocus() {
        toolbarView.view.clearFocus()
    }

    @ExperimentalCoroutinesApi
    @SuppressWarnings("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        search_scan_button.visibility = if (context?.hasCamera() == true) View.VISIBLE else View.GONE

        qrFeature.set(
            QrFeature(
                requireContext(),
                fragmentManager = parentFragmentManager,
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
                },
                onScanResult = { result ->
                    search_scan_button.isChecked = false
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

        view.search_scan_button.setOnClickListener {
            toolbarView.view.clearFocus()
            requireComponents.analytics.metrics.track(Event.QRScannerOpened)
            qrFeature.get()?.scan(R.id.container)
        }

        view.search_shortcuts_button.setOnClickListener {
            searchInteractor.onSearchShortcutsButtonClicked()
        }

        val stubListener = ViewStub.OnInflateListener { _, inflated ->
            inflated.learn_more.setOnClickListener {
                (activity as HomeActivity)
                    .openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                            SupportUtils.SumoTopic.SEARCH_SUGGESTION
                        ),
                        newTab = searchStore.state.session == null,
                        from = BrowserDirection.FromSearch
                    )
            }

            inflated.allow.setOnClickListener {
                inflated.visibility = View.GONE
                context?.settings()?.shouldShowSearchSuggestionsInPrivate = true
                context?.settings()?.showSearchSuggestionsInPrivateOnboardingFinished = true
                searchStore.dispatch(SearchFragmentAction.SetShowSearchSuggestions(true))
                searchStore.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(false))
                requireComponents.analytics.metrics.track(Event.PrivateBrowsingShowSearchSuggestions)
            }

            inflated.dismiss.setOnClickListener {
                inflated.visibility = View.GONE
                context?.settings()?.shouldShowSearchSuggestionsInPrivate = false
                context?.settings()?.showSearchSuggestionsInPrivateOnboardingFinished = true
            }

            inflated.text.text =
                getString(R.string.search_suggestions_onboarding_text, getString(R.string.app_name))

            inflated.title.text =
                getString(R.string.search_suggestions_onboarding_title)
        }

        view.search_suggestions_onboarding.setOnInflateListener((stubListener))

        view.toolbar_wrapper.clipToOutline = false

        fill_link_from_clipboard.setOnClickListener {
            (activity as HomeActivity)
                .openToBrowserAndLoad(
                    searchTermOrURL = requireContext().components.clipboardHandler.url ?: "",
                    newTab = searchStore.state.session == null,
                    from = BrowserDirection.FromSearch
                )
        }

        consumeFrom(searchStore) {
            awesomeBarView.update(it)
            updateSearchShortcutsIcon(it)
            toolbarView.update(it)
            updateSearchWithLabel(it)
            updateClipboardSuggestion(it, requireContext().components.clipboardHandler.url)
            updateSearchSuggestionsHintVisibility(it)
        }

        startPostponedEnterTransition()
    }

    override fun onResume() {
        super.onResume()

        // The user has the option to go to 'Shortcuts' -> 'Search engine settings' to modify the default search engine.
        // When returning from that settings screen we need to update it to account for any changes.
        val currentDefaultEngine =
            requireComponents.search.provider.getDefaultEngine(requireContext())

        if (searchStore.state.defaultEngineSource.searchEngine != currentDefaultEngine) {
            searchStore.dispatch(
                SearchFragmentAction.SelectNewDefaultSearchEngine
                    (currentDefaultEngine)
            )
        }

        if (!permissionDidUpdate) {
            toolbarView.view.edit.focus()
        }

        updateClipboardSuggestion(
            searchStore.state,
            requireContext().components.clipboardHandler.url
        )

        permissionDidUpdate = false
        hideToolbar()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, intent: Intent?) {
        if (requestCode == SPEECH_REQUEST_CODE && resultCode == RESULT_OK) {
            intent?.getStringArrayListExtra(EXTRA_RESULTS)?.first()?.also {
                toolbarView.view.edit.updateUrl(url = it, shouldHighlight = true)
                searchInteractor.onTextChanged(it)
                toolbarView.view.edit.focus()
            }
        }
    }

    override fun onPause() {
        super.onPause()
        toolbarView.view.clearFocus()
    }

    override fun onBackPressed(): Boolean {
        // Note: Actual navigation happens in `handleEditingCancelled` in SearchController
        return when {
            qrFeature.onBackPressed() -> {
                toolbarView.view.edit.focus()
                view?.search_scan_button?.isChecked = false
                toolbarView.view.requestFocus()
            }
            else -> true
        }
    }

    private fun updateSearchWithLabel(searchState: SearchFragmentState) {
        search_with_shortcuts.visibility =
            if (searchState.showSearchShortcuts) View.VISIBLE else View.GONE
    }

    private fun updateClipboardSuggestion(searchState: SearchFragmentState, clipboardUrl: String?) {
        val visibility =
            if (searchState.showClipboardSuggestions && searchState.query.isEmpty() && !clipboardUrl.isNullOrEmpty())
                View.VISIBLE else View.GONE

        fill_link_from_clipboard.visibility = visibility
        divider_line.visibility = visibility
        clipboard_url.text = clipboardUrl

        if (clipboardUrl != null && !((activity as HomeActivity).browsingModeManager.mode.isPrivate)) {
            requireComponents.core.engine.speculativeConnect(clipboardUrl)
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
                        view?.search_scan_button?.isChecked = false
                    }
                }
            }
            else -> super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        }
    }

    private fun historyStorageProvider(): HistoryStorage? {
        return if (requireContext().settings().shouldShowHistorySuggestions) {
            requireComponents.core.historyStorage
        } else null
    }

    private fun updateSearchSuggestionsHintVisibility(state: SearchFragmentState) {
        view?.apply {
            findViewById<View>(R.id.search_suggestions_onboarding)?.isVisible = state.showSearchSuggestionsHint

            search_suggestions_onboarding_divider?.isVisible =
                search_with_shortcuts.isVisible && state.showSearchSuggestionsHint
        }
    }

    private fun updateSearchShortcutsIcon(searchState: SearchFragmentState) {
        view?.apply {
            val showShortcuts = searchState.showSearchShortcuts
            search_shortcuts_button.isChecked = showShortcuts

            val color = if (showShortcuts) R.attr.contrastText else R.attr.primaryText
            search_shortcuts_button.compoundDrawables[0]?.setTint(
                requireContext().getColorFromAttr(color)
            )
        }
    }

    companion object {
        private const val SHARED_TRANSITION_MS = 250L
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
    }
}
