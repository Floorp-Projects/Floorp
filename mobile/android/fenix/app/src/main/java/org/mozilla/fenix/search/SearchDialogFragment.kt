/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.Manifest
import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.ColorDrawable
import android.os.Build
import android.os.Bundle
import android.speech.RecognizerIntent
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewStub
import android.view.WindowManager
import android.view.accessibility.AccessibilityEvent
import android.view.inputmethod.InputMethodManager
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.content.res.AppCompatResources
import androidx.constraintlayout.widget.ConstraintProperties.BOTTOM
import androidx.constraintlayout.widget.ConstraintProperties.PARENT_ID
import androidx.constraintlayout.widget.ConstraintProperties.TOP
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.view.isVisible
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.qr.QrFeature
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.hasCamera
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.ktx.android.content.res.getSpanned
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.FragmentSearchDialogBinding
import org.mozilla.fenix.databinding.SearchSuggestionsHintBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.toolbar.ToolbarView
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.widget.VoiceSearchActivity

typealias SearchDialogFragmentStore = SearchFragmentStore

@SuppressWarnings("LargeClass", "TooManyFunctions")
class SearchDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {
    private var _binding: FragmentSearchDialogBinding? = null
    private val binding get() = _binding!!

    private var voiceSearchButtonAlreadyAdded: Boolean = false
    private lateinit var interactor: SearchDialogInteractor
    private lateinit var store: SearchDialogFragmentStore
    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView
    private var firstUpdate = true

    private val qrFeature = ViewBoundFeatureWrapper<QrFeature>()
    private val speechIntent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
    private var dialogHandledAction = false

    override fun onStart() {
        super.onStart()

        if (FeatureFlags.showHomeBehindSearch) {
            // This will need to be handled for the update to R. We need to resize here in order to
            // see the whole homescreen behind the search dialog.
            @Suppress("DEPRECATION")
            requireActivity().window.setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
            )
        } else {
            // https://github.com/mozilla-mobile/fenix/issues/14279
            // To prevent GeckoView from resizing we're going to change the softInputMode to not adjust
            // the size of the window.
            requireActivity().window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING)
        }
        // Refocus the toolbar editing and show keyboard if the QR fragment isn't showing
        if (childFragmentManager.findFragmentByTag(QR_FRAGMENT_TAG) == null) {
            toolbarView.view.edit.focus()
        }
    }

    override fun onStop() {
        super.onStop()
        // https://github.com/mozilla-mobile/fenix/issues/14279
        // Let's reset back to the default behavior after we're done searching
        // This will be addressed on https://github.com/mozilla-mobile/fenix/issues/17805
        @Suppress("DEPRECATION")
        requireActivity().window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE)
    }

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

    @SuppressWarnings("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val args by navArgs<SearchDialogFragmentArgs>()
        _binding = FragmentSearchDialogBinding.inflate(inflater, container, false)
        val activity = requireActivity() as HomeActivity
        val isPrivate = activity.browsingModeManager.mode.isPrivate

        requireComponents.analytics.metrics.track(Event.InteractWithSearchURLArea)

        store = SearchDialogFragmentStore(
            createInitialSearchFragmentState(
                activity,
                requireComponents,
                tabId = args.sessionId,
                pastedText = args.pastedText,
                searchAccessPoint = args.searchAccessPoint
            )
        )

        interactor = SearchDialogInteractor(
            SearchDialogController(
                activity = activity,
                store = requireComponents.core.store,
                tabsUseCases = requireComponents.useCases.tabsUseCases,
                fragmentStore = store,
                navController = findNavController(),
                settings = requireContext().settings(),
                metrics = requireComponents.analytics.metrics,
                dismissDialog = {
                    dialogHandledAction = true
                    dismissAllowingStateLoss()
                },
                clearToolbarFocus = {
                    dialogHandledAction = true
                    toolbarView.view.hideKeyboard()
                    toolbarView.view.clearFocus()
                },
                focusToolbar = { toolbarView.view.edit.focus() },
                clearToolbar = {
                    toolbarView.view
                        .findViewById<InlineAutocompleteEditText>(R.id.mozac_browser_toolbar_edit_url_view)
                        ?.setText("")
                }
            )
        )

        val fromHomeFragment =
            findNavController().previousBackStackEntry?.destination?.id == R.id.homeFragment

        toolbarView = ToolbarView(
            requireContext(),
            interactor,
            historyStorageProvider(),
            isPrivate,
            binding.toolbar,
            requireComponents.core.engine,
            fromHomeFragment
        )

        val awesomeBar = binding.awesomeBar
        awesomeBar.customizeForBottomToolbar = requireContext().settings().shouldUseBottomToolbar

        awesomeBarView = AwesomeBarView(
            activity,
            interactor,
            awesomeBar,
            fromHomeFragment
        )

        binding.awesomeBar.setOnTouchListener { _, _ ->
            binding.root.hideKeyboard()
            false
        }

        awesomeBarView.view.setOnEditSuggestionListener(toolbarView.view::setSearchTerms)

        val urlView = toolbarView.view
            .findViewById<InlineAutocompleteEditText>(R.id.mozac_browser_toolbar_edit_url_view)
        urlView?.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO

        requireComponents.core.engine.speculativeCreateSession(isPrivate)

        if (fromHomeFragment) {
            // When displayed above home, dispatches the touch events to scrim area to the HomeFragment
            binding.searchWrapper.background = ColorDrawable(Color.TRANSPARENT)
            dialog?.window?.decorView?.setOnTouchListener { _, event ->
                requireActivity().dispatchTouchEvent(event)
                false
            }
        }

        return binding.root
    }

    @ExperimentalCoroutinesApi
    @SuppressWarnings("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFlow(requireComponents.core.store) { flow ->
            flow.map { state -> state.search }
                .ifChanged()
                .collect { search ->
                    store.dispatch(SearchFragmentAction.UpdateSearchState(search))
                }
        }

        setupConstraints(view)

        // When displayed above browser, dismisses dialog on clicking scrim area
        if (findNavController().previousBackStackEntry?.destination?.id == R.id.browserFragment) {
            binding.searchWrapper.setOnClickListener {
                it.hideKeyboard()
                dismissAllowingStateLoss()
            }
        }

        binding.searchEnginesShortcutButton.setOnClickListener {
            interactor.onSearchShortcutsButtonClicked()
        }

        qrFeature.set(
            createQrFeature(),
            owner = this,
            view = view
        )

        binding.qrScanButton.visibility = if (context?.hasCamera() == true) View.VISIBLE else View.GONE

        binding.qrScanButton.setOnClickListener {
            if (!requireContext().hasCamera()) { return@setOnClickListener }
            view.hideKeyboard()
            toolbarView.view.clearFocus()

            if (requireContext().settings().shouldShowCameraPermissionPrompt) {
                qrFeature.get()?.scan(binding.searchWrapper.id)
            } else {
                if (requireContext().isPermissionGranted(Manifest.permission.CAMERA)) {
                    qrFeature.get()?.scan(binding.searchWrapper.id)
                } else {
                    interactor.onCameraPermissionsNeeded()
                    resetFocus()
                    view.hideKeyboard()
                    toolbarView.view.requestFocus()
                }
            }
            requireContext().settings().setCameraPermissionNeededState = false
        }

        binding.fillLinkFromClipboard.setOnClickListener {
            requireComponents.analytics.metrics.track(Event.ClipboardSuggestionClicked)
            view.hideKeyboard()
            toolbarView.view.clearFocus()
            (activity as HomeActivity)
                .openToBrowserAndLoad(
                    searchTermOrURL = requireContext().components.clipboardHandler.url ?: "",
                    newTab = store.state.tabId == null,
                    from = BrowserDirection.FromSearchDialog
                )
        }

        val stubListener = ViewStub.OnInflateListener { _, inflated ->
            val searchSuggestionHintBinding = SearchSuggestionsHintBinding.bind(inflated)

            searchSuggestionHintBinding.learnMore.setOnClickListener {
                (activity as HomeActivity)
                    .openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                            SupportUtils.SumoTopic.SEARCH_SUGGESTION
                        ),
                        newTab = store.state.tabId == null,
                        from = BrowserDirection.FromSearchDialog
                    )
            }

            searchSuggestionHintBinding.allow.setOnClickListener {
                inflated.visibility = View.GONE
                requireContext().settings().also {
                    it.shouldShowSearchSuggestionsInPrivate = true
                    it.showSearchSuggestionsInPrivateOnboardingFinished = true
                }
                store.dispatch(SearchFragmentAction.SetShowSearchSuggestions(true))
                store.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(false))
                requireComponents.analytics.metrics.track(Event.PrivateBrowsingShowSearchSuggestions)
            }

            searchSuggestionHintBinding.dismiss.setOnClickListener {
                inflated.visibility = View.GONE
                requireContext().settings().also {
                    it.shouldShowSearchSuggestionsInPrivate = false
                    it.showSearchSuggestionsInPrivateOnboardingFinished = true
                }
            }

            searchSuggestionHintBinding.text.text =
                getString(R.string.search_suggestions_onboarding_text, getString(R.string.app_name))

            searchSuggestionHintBinding.title.text =
                getString(R.string.search_suggestions_onboarding_title)
        }

        binding.searchSuggestionsHint.setOnInflateListener((stubListener))
        if (view.context.settings().accessibilityServicesEnabled) {
            updateAccessibilityTraversalOrder()
        }

        consumeFrom(store) {
            /*
            * firstUpdate is used to make sure we keep the awesomebar hidden on the first run
            *  of the searchFragmentDialog. We only turn it false after the user has changed the
            *  query as consumeFrom may run several times on fragment start due to state updates.
            * */
            if (it.url != it.query) firstUpdate = false
            binding.awesomeBar.visibility = if (shouldShowAwesomebar(it)) View.VISIBLE else View.INVISIBLE
            updateSearchSuggestionsHintVisibility(it)
            updateClipboardSuggestion(it, requireContext().components.clipboardHandler.url)
            updateToolbarContentDescription(it)
            updateSearchShortcutsIcon(it)
            toolbarView.update(it)
            awesomeBarView.update(it)
            addVoiceSearchButton(it)
        }
    }

    private fun shouldShowAwesomebar(searchFragmentState: SearchFragmentState) =
        !firstUpdate && searchFragmentState.query.isNotBlank() || searchFragmentState.showSearchShortcuts

    private fun updateAccessibilityTraversalOrder() {
        val searchWrapperId = binding.searchWrapper.id
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            binding.qrScanButton.accessibilityTraversalAfter = searchWrapperId
            binding.searchEnginesShortcutButton.accessibilityTraversalAfter = searchWrapperId
            binding.fillLinkFromClipboard.accessibilityTraversalAfter = searchWrapperId
        } else {
            viewLifecycleOwner.lifecycleScope.launch {
                binding.searchWrapper.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED)
            }
        }
    }

    override fun onPause() {
        super.onPause()
        view?.hideKeyboard()
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _binding = null
    }

    /*
     * This way of dismissing the keyboard is needed to smoothly dismiss the keyboard while the dialog
     * is also dismissing. For example, when clicking a top site on home while this dialog is showing.
     */
    private fun hideDeviceKeyboard() {
        // If the interactor/controller has handled a search event itself, it will hide the keyboard.
        if (!dialogHandledAction) {
            val imm =
                requireContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.toggleSoftInput(InputMethodManager.HIDE_IMPLICIT_ONLY, 0)
        }
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        hideDeviceKeyboard()
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
                resetFocus()
                true
            }
            else -> {
                if (FeatureFlags.showHomeBehindSearch) {
                    val args by navArgs<SearchDialogFragmentArgs>()
                    args.sessionId?.let {
                        findNavController().navigate(
                            SearchDialogFragmentDirections.actionGlobalBrowser(null)
                        )
                    }
                }
                view?.hideKeyboard()
                dismissAllowingStateLoss()
                true
            }
        }
    }

    private fun historyStorageProvider(): HistoryStorage? {
        return if (requireContext().settings().shouldShowHistorySuggestions) {
            requireComponents.core.historyStorage
        } else null
    }

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19920
    private fun createQrFeature(): QrFeature {
        return QrFeature(
            requireContext(),
            fragmentManager = childFragmentManager,
            onNeedToRequestPermissions = { permissions ->
                requestPermissions(permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
            },
            onScanResult = { result ->
                binding.qrScanButton.isChecked = false
                activity?.let {
                    AlertDialog.Builder(it).apply {
                        val spannable = resources.getSpanned(
                            R.string.qr_scanner_confirmation_dialog_message,
                            getString(R.string.app_name) to StyleSpan(Typeface.BOLD),
                            result to StyleSpan(Typeface.ITALIC)
                        )
                        setMessage(spannable)
                        setNegativeButton(R.string.qr_scanner_dialog_negative) { dialog: DialogInterface, _ ->
                            dialog.cancel()
                        }
                        setPositiveButton(R.string.qr_scanner_dialog_positive) { dialog: DialogInterface, _ ->
                            (activity as? HomeActivity)?.openToBrowserAndLoad(
                                searchTermOrURL = result,
                                newTab = store.state.tabId == null,
                                from = BrowserDirection.FromSearchDialog
                            )
                            dialog.dismiss()
                        }
                        create()
                    }.show()
                }
            }
        )
    }

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19920
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        when (requestCode) {
            REQUEST_CODE_CAMERA_PERMISSIONS -> qrFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
                resetFocus()
                requireContext().settings().setCameraPermissionNeededState = false
            }
            else -> super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        }
    }

    private fun resetFocus() {
        binding.qrScanButton.isChecked = false
        toolbarView.view.edit.focus()
        toolbarView.view.requestFocus()
    }

    private fun setupConstraints(view: View) {
        if (view.context.settings().toolbarPosition == ToolbarPosition.BOTTOM) {
            ConstraintSet().apply {
                clone(binding.searchWrapper)

                clear(binding.toolbar.id, TOP)
                connect(binding.toolbar.id, BOTTOM, PARENT_ID, BOTTOM)

                clear(binding.pillWrapper.id, BOTTOM)
                connect(binding.pillWrapper.id, BOTTOM, binding.toolbar.id, TOP)

                clear(binding.awesomeBar.id, TOP)
                clear(binding.awesomeBar.id, BOTTOM)
                connect(binding.awesomeBar.id, TOP, binding.searchSuggestionsHint.id, BOTTOM)
                connect(binding.awesomeBar.id, BOTTOM, binding.pillWrapper.id, TOP)

                clear(binding.searchSuggestionsHint.id, TOP)
                clear(binding.searchSuggestionsHint.id, BOTTOM)
                connect(binding.searchSuggestionsHint.id, TOP, PARENT_ID, TOP)
                connect(binding.searchSuggestionsHint.id, BOTTOM, binding.searchHintBottomBarrier.id, TOP)

                clear(binding.fillLinkFromClipboard.id, TOP)
                connect(binding.fillLinkFromClipboard.id, BOTTOM, binding.pillWrapper.id, TOP)

                clear(binding.fillLinkDivider.id, TOP)
                connect(binding.fillLinkDivider.id, BOTTOM, binding.fillLinkFromClipboard.id, TOP)

                applyTo(binding.searchWrapper)
            }
        }
    }

    private fun updateSearchSuggestionsHintVisibility(state: SearchFragmentState) {
        view?.apply {
            val showHint = state.showSearchSuggestionsHint &&
                !state.showSearchShortcuts &&
                state.url != state.query

            binding.searchSuggestionsHint.isVisible = showHint
            binding.searchSuggestionsHintDivider.isVisible = showHint
        }
    }

    private fun addVoiceSearchButton(searchFragmentState: SearchFragmentState) {
        if (voiceSearchButtonAlreadyAdded) return
        val searchEngine = searchFragmentState.searchEngineSource.searchEngine

        val isVisible =
            searchEngine?.id?.contains("google") == true &&
                isSpeechAvailable() &&
                requireContext().settings().shouldShowVoiceSearch

        if (isVisible) {
            toolbarView.view.addEditAction(
                BrowserToolbar.Button(
                    AppCompatResources.getDrawable(requireContext(), R.drawable.ic_microphone)!!,
                    requireContext().getString(R.string.voice_search_content_description),
                    visible = { true },
                    listener = ::launchVoiceSearch
                )
            )
            voiceSearchButtonAlreadyAdded = true
        }
    }

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19919
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

    private fun updateClipboardSuggestion(searchState: SearchFragmentState, clipboardUrl: String?) {
        val shouldShowView = searchState.showClipboardSuggestions &&
            searchState.query.isEmpty() &&
            !clipboardUrl.isNullOrEmpty() && !searchState.showSearchShortcuts

        binding.fillLinkFromClipboard.isVisible = shouldShowView
        binding.fillLinkDivider.isVisible = shouldShowView
        binding.pillWrapperDivider.isVisible =
            !(shouldShowView && requireComponents.settings.shouldUseBottomToolbar)
        binding.clipboardUrl.isVisible = shouldShowView
        binding.clipboardTitle.isVisible = shouldShowView
        binding.linkIcon.isVisible = shouldShowView

        binding.clipboardUrl.text = clipboardUrl

        binding.fillLinkFromClipboard.contentDescription =
            "${binding.clipboardTitle.text}, ${binding.clipboardUrl.text}."

        if (clipboardUrl != null && !((activity as HomeActivity).browsingModeManager.mode.isPrivate)) {
            requireComponents.core.engine.speculativeConnect(clipboardUrl)
        }
    }

    private fun updateToolbarContentDescription(searchState: SearchFragmentState) {
        val urlView = toolbarView.view
            .findViewById<InlineAutocompleteEditText>(R.id.mozac_browser_toolbar_edit_url_view)

        searchState.searchEngineSource.searchEngine?.let { engine ->
            toolbarView.view.contentDescription = engine.name + ", " + urlView.hint
        }

        urlView?.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
    }

    private fun updateSearchShortcutsIcon(searchState: SearchFragmentState) {
        view?.apply {
            binding.searchEnginesShortcutButton.isVisible = searchState.areShortcutsAvailable

            val showShortcuts = searchState.showSearchShortcuts
            binding.searchEnginesShortcutButton.isChecked = showShortcuts

            val color = if (showShortcuts) R.attr.contrastText else R.attr.primaryText
            binding.searchEnginesShortcutButton.compoundDrawables[0]?.setTint(
                requireContext().getColorFromAttr(color)
            )
        }
    }

    companion object {
        private const val QR_FRAGMENT_TAG = "MOZAC_QR_FRAGMENT"
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
    }
}
