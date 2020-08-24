/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.searchdialog

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.graphics.Typeface
import android.os.Bundle
import android.speech.RecognizerIntent
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewStub
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.constraintlayout.widget.ConstraintProperties.BOTTOM
import androidx.constraintlayout.widget.ConstraintProperties.PARENT_ID
import androidx.constraintlayout.widget.ConstraintProperties.TOP
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_search_dialog.*
import kotlinx.android.synthetic.main.fragment_search_dialog.fill_link_from_clipboard
import kotlinx.android.synthetic.main.fragment_search_dialog.pill_wrapper
import kotlinx.android.synthetic.main.fragment_search_dialog.qr_scan_button
import kotlinx.android.synthetic.main.fragment_search_dialog.toolbar
import kotlinx.android.synthetic.main.fragment_search_dialog.view.*
import kotlinx.android.synthetic.main.search_suggestions_onboarding.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.qr.QrFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.hasCamera
import mozilla.components.support.ktx.android.content.res.getSpanned
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore
import org.mozilla.fenix.components.searchengine.FenixSearchEngineProvider
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.SearchFragmentAction
import org.mozilla.fenix.search.SearchFragmentState
import org.mozilla.fenix.search.SearchFragmentStore
import org.mozilla.fenix.search.SearchInteractor
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.createInitialSearchFragmentState
import org.mozilla.fenix.search.toolbar.ToolbarView
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.registerOnSharedPreferenceChangeListener
import org.mozilla.fenix.widget.VoiceSearchActivity

typealias SearchDialogFragmentStore = SearchFragmentStore
typealias SearchDialogInteractor = SearchInteractor

@SuppressWarnings("LargeClass", "TooManyFunctions")
class SearchDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {
    private lateinit var interactor: SearchDialogInteractor
    private lateinit var store: SearchDialogFragmentStore
    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView
    private var firstUpdate = true

    private val qrFeature = ViewBoundFeatureWrapper<QrFeature>()
    private val speechIntent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.SearchDialogStyle)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return object : Dialog(requireContext(), this.theme) {
            override fun onBackPressed() {
                this@SearchDialogFragment.onBackPressed()
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val args by navArgs<SearchDialogFragmentArgs>()
        val view = inflater.inflate(R.layout.fragment_search_dialog, container, false)

        store = SearchDialogFragmentStore(
            createInitialSearchFragmentState(
                activity as HomeActivity,
                requireComponents,
                tabId = args.sessionId,
                pastedText = args.pastedText,
                searchAccessPoint = args.searchAccessPoint
            )
        )

        interactor = SearchDialogInteractor(
            SearchDialogController(
                activity = requireActivity() as HomeActivity,
                sessionManager = requireComponents.core.sessionManager,
                store = store,
                navController = findNavController(),
                settings = requireContext().settings(),
                metrics = requireComponents.analytics.metrics,
                clearToolbarFocus = {
                    toolbarView.view.hideKeyboard()
                    toolbarView.view.clearFocus()
                }
            )
        )

        toolbarView = ToolbarView(
            requireContext(),
            interactor,
            null,
            false,
            view.toolbar,
            requireComponents.core.engine
        ).also(::addSearchButton)

        awesomeBarView = AwesomeBarView(
            requireContext(),
            interactor,
            view.awesome_bar
        )

        setShortcutsChangedListener(CustomSearchEngineStore.PREF_FILE_SEARCH_ENGINES)
        setShortcutsChangedListener(FenixSearchEngineProvider.PREF_FILE_SEARCH_ENGINES)

        view.awesome_bar.setOnTouchListener { _, _ ->
            view.hideKeyboard()
            false
        }

        awesomeBarView.view.setOnEditSuggestionListener(toolbarView.view::setSearchTerms)

        val urlView = toolbarView.view
            .findViewById<InlineAutocompleteEditText>(R.id.mozac_browser_toolbar_edit_url_view)
        urlView?.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO

        val isPrivate = (requireActivity() as HomeActivity).browsingModeManager.mode.isPrivate
        requireComponents.core.engine.speculativeCreateSession(isPrivate)

        return view
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupConstraints(view)

        search_wrapper.setOnClickListener {
            it.hideKeyboard()
            dismissAllowingStateLoss()
        }

        view.search_engines_shortcut_button.setOnClickListener {
            interactor.onSearchShortcutsButtonClicked()
        }

        qr_scan_button.visibility = if (context?.hasCamera() == true) View.VISIBLE else View.GONE

        qr_scan_button.setOnClickListener {
            if (!requireContext().hasCamera()) { return@setOnClickListener }

            toolbarView.view.clearFocus()
            requireComponents.analytics.metrics.track(Event.QRScannerOpened)
            qrFeature.get()?.scan(R.id.search_wrapper)
        }

        fill_link_from_clipboard.setOnClickListener {
            (activity as HomeActivity)
                .openToBrowserAndLoad(
                    searchTermOrURL = requireContext().components.clipboardHandler.url ?: "",
                    newTab = store.state.tabId == null,
                    from = BrowserDirection.FromSearchDialog
                )
        }

        qrFeature.set(
            createQrFeature(),
            owner = this,
            view = view
        )

        val stubListener = ViewStub.OnInflateListener { _, inflated ->
            inflated.learn_more.setOnClickListener {
                (activity as HomeActivity)
                    .openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                            SupportUtils.SumoTopic.SEARCH_SUGGESTION
                        ),
                        newTab = store.state.tabId == null,
                        from = BrowserDirection.FromSearch
                    )
            }

            inflated.allow.setOnClickListener {
                inflated.visibility = View.GONE
                context?.settings()?.shouldShowSearchSuggestionsInPrivate = true
                context?.settings()?.showSearchSuggestionsInPrivateOnboardingFinished = true
                store.dispatch(SearchFragmentAction.SetShowSearchSuggestions(true))
                store.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(false))
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

        consumeFrom(store) {
            val shouldShowAwesomebar =
                !firstUpdate &&
                it.query.isNotBlank() ||
                it.showSearchShortcuts

            awesome_bar?.visibility = if (shouldShowAwesomebar) View.VISIBLE else View.INVISIBLE
            updateSearchSuggestionsHintVisibility(it)
            updateClipboardSuggestion(it, requireContext().components.clipboardHandler.url)
            toolbarView.update(it)
            awesomeBarView.update(it)
            firstUpdate = false
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, intent: Intent?) {
        if (requestCode == VoiceSearchActivity.SPEECH_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            intent?.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS)?.first()?.also {
                toolbarView.view.edit.updateUrl(url = it, shouldHighlight = true)
                interactor.onTextChanged(it)
                toolbarView.view.edit.focus()
            }
        }
    }

    override fun onBackPressed(): Boolean {
        return when {
            qrFeature.onBackPressed() -> {
                toolbarView.view.edit.focus()
                view?.qr_scan_button?.isChecked = false
                toolbarView.view.requestFocus()
                true
            }
            else -> {
                view?.hideKeyboard()
                dismissAllowingStateLoss()
                true
            }
        }
    }

    private fun createQrFeature(): QrFeature {
        return QrFeature(
            requireContext(),
            fragmentManager = childFragmentManager,
            onNeedToRequestPermissions = { permissions ->
                requestPermissions(permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
            },
            onScanResult = { result ->
                qr_scan_button.isChecked = false
                activity?.let {
                    AlertDialog.Builder(it).apply {
                        val spannable = resources.getSpanned(
                            R.string.qr_scanner_confirmation_dialog_message,
                            getString(R.string.app_name) to StyleSpan(Typeface.BOLD),
                            result to StyleSpan(Typeface.ITALIC)
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
                                    newTab = store.state.tabId == null,
                                    from = BrowserDirection.FromSearch
                                )
                            dialog.dismiss()
                        }
                        create()
                    }.show()
                    requireComponents.analytics.metrics.track(Event.QRScannerPromptDisplayed)
                }
            })
    }

    private fun setupConstraints(view: View) {
        if (view.context.settings().toolbarPosition == ToolbarPosition.BOTTOM) {
            ConstraintSet().apply {
                clone(search_wrapper)

                clear(toolbar.id, TOP)
                connect(toolbar.id, BOTTOM, PARENT_ID, BOTTOM)

                clear(awesome_bar.id, TOP)
                clear(pill_wrapper.id, BOTTOM)
                connect(awesome_bar.id, TOP, PARENT_ID, TOP)
                connect(pill_wrapper.id, BOTTOM, toolbar.id, TOP)

                clear(fill_link_from_clipboard.id, TOP)
                connect(fill_link_from_clipboard.id, BOTTOM, pill_wrapper.id, TOP)

                applyTo(search_wrapper)
            }
        }
    }

    private fun updateSearchSuggestionsHintVisibility(state: SearchFragmentState) {
        view?.apply {
            findViewById<View>(R.id.search_suggestions_onboarding)?.isVisible = state.showSearchSuggestionsHint
            search_suggestions_onboarding_divider?.isVisible = state.showSearchSuggestionsHint
        }
    }

    private fun addSearchButton(toolbarView: ToolbarView) {
        toolbarView.view.addEditAction(
            BrowserToolbar.Button(
                ContextCompat.getDrawable(requireContext(), R.drawable.ic_microphone)!!,
                requireContext().getString(R.string.voice_search_content_description),
                visible = {
                    store.state.searchEngineSource.searchEngine.identifier.contains("google") &&
                            isSpeechAvailable() &&
                            requireContext().settings().shouldShowVoiceSearch
                },
                listener = ::launchVoiceSearch
            )
        )
    }

    private fun launchVoiceSearch() {
        // Note if a user disables speech while the app is on the search fragment
        // the voice button will still be available and *will* cause a crash if tapped,
        // since the `visible` call is only checked on create. In order to avoid extra complexity
        // around such a small edge case, we make the button have no functionality in this case.
        if (!isSpeechAvailable()) { return }

        requireComponents.analytics.metrics.track(Event.VoiceSearchTapped)
        speechIntent.apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_PROMPT, requireContext().getString(R.string.voice_search_explainer))
        }
        startActivityForResult(speechIntent, VoiceSearchActivity.SPEECH_REQUEST_CODE)
    }

    private fun isSpeechAvailable(): Boolean = speechIntent.resolveActivity(requireContext().packageManager) != null

    private fun setShortcutsChangedListener(preferenceFileName: String) {
        requireContext().getSharedPreferences(
            preferenceFileName,
            Context.MODE_PRIVATE
        ).registerOnSharedPreferenceChangeListener(viewLifecycleOwner) { _, _ ->
            awesomeBarView.update(store.state)
        }
    }

    private fun updateClipboardSuggestion(searchState: SearchFragmentState, clipboardUrl: String?) {
        val visibility =
            if (searchState.showClipboardSuggestions && searchState.query.isEmpty() && !clipboardUrl.isNullOrEmpty())
                View.VISIBLE else View.GONE

        fill_link_from_clipboard.visibility = visibility
        clipboard_url.text = clipboardUrl

        if (clipboardUrl != null && !((activity as HomeActivity).browsingModeManager.mode.isPrivate)) {
            requireComponents.core.engine.speculativeConnect(clipboardUrl)
        }
    }

    companion object {
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
    }
}
