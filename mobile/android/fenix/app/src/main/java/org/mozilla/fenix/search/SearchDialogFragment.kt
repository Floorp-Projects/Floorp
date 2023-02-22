/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.Manifest
import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.ColorDrawable
import android.os.Build
import android.os.Bundle
import android.speech.RecognizerIntent
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.ViewStub
import android.view.WindowManager
import android.view.accessibility.AccessibilityEvent
import android.view.inputmethod.InputMethodManager
import androidx.activity.result.ActivityResultLauncher
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.content.res.AppCompatResources
import androidx.constraintlayout.widget.ConstraintProperties.BOTTOM
import androidx.constraintlayout.widget.ConstraintProperties.PARENT_ID
import androidx.constraintlayout.widget.ConstraintProperties.TOP
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.graphics.drawable.toDrawable
import androidx.core.net.toUri
import androidx.core.view.isVisible
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavGraph
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.qr.QrFeature
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.coroutines.Dispatchers
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.hasCamera
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.ktx.android.content.res.getSpanned
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.android.view.findViewInHierarchy
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Awesomebar
import org.mozilla.fenix.GleanMetrics.VoiceSearch
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Core.Companion.BOOKMARKS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.Core.Companion.HISTORY_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.FragmentSearchDialogBinding
import org.mozilla.fenix.databinding.SearchSuggestionsHintBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRectWithScreenLocation
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.registerForActivityResult
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.secure
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.awesomebar.toSearchProviderState
import org.mozilla.fenix.search.toolbar.IncreasedTapAreaActionDecorator
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu
import org.mozilla.fenix.search.toolbar.SearchSelectorToolbarAction
import org.mozilla.fenix.search.toolbar.ToolbarView
import org.mozilla.fenix.settings.SupportUtils

typealias SearchDialogFragmentStore = SearchFragmentStore

@SuppressWarnings("LargeClass", "TooManyFunctions")
class SearchDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {
    private var _binding: FragmentSearchDialogBinding? = null
    private val binding get() = _binding!!

    @VisibleForTesting internal lateinit var interactor: SearchDialogInteractor
    private lateinit var store: SearchDialogFragmentStore
    private lateinit var toolbarView: ToolbarView
    private lateinit var inlineAutocompleteEditText: InlineAutocompleteEditText
    private lateinit var awesomeBarView: AwesomeBarView
    private lateinit var startForResult: ActivityResultLauncher<Intent>

    private val searchSelectorMenu by lazy {
        SearchSelectorMenu(
            context = requireContext(),
            interactor = interactor,
        )
    }

    private val qrFeature = ViewBoundFeatureWrapper<QrFeature>()
    private val speechIntent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)

    private var isPrivateButtonClicked = false
    private var dialogHandledAction = false
    private var searchSelectorAlreadyAdded = false
    private var qrButtonAction: Toolbar.Action? = null
    private var voiceSearchButtonAction: Toolbar.Action? = null

    override fun onStart() {
        super.onStart()

        // This will need to be handled for the update to R. We need to resize here in order to
        // see the whole homescreen behind the search dialog.
        @Suppress("DEPRECATION")
        requireActivity().window.setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE,
        )

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

        startForResult = registerForActivityResult { result ->
            result.data?.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS)?.first()?.also {
                toolbarView.view.edit.updateUrl(url = it, shouldHighlight = true)
                interactor.onTextChanged(it)
                toolbarView.view.edit.focus()
            }
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return object : Dialog(requireContext(), this.theme) {
            @Deprecated("Deprecated in Java")
            override fun onBackPressed() {
                this@SearchDialogFragment.onBackPressed()
            }
        }.apply {
            if ((requireActivity() as HomeActivity).browsingModeManager.mode.isPrivate) {
                this.secure(requireActivity())
            }
        }
    }

    @SuppressWarnings("LongMethod")
    @SuppressLint("ClickableViewAccessibility")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val args by navArgs<SearchDialogFragmentArgs>()
        _binding = FragmentSearchDialogBinding.inflate(inflater, container, false)
        val activity = requireActivity() as HomeActivity
        val isPrivate = activity.browsingModeManager.mode.isPrivate

        store = SearchDialogFragmentStore(
            createInitialSearchFragmentState(
                activity,
                requireComponents,
                tabId = args.sessionId,
                pastedText = args.pastedText,
                searchAccessPoint = args.searchAccessPoint,
                searchEngine = requireComponents.core.store.state.search.searchEngines.firstOrNull {
                    it.id == args.searchEngine
                },
            ),
        )

        interactor = SearchDialogInteractor(
            SearchDialogController(
                activity = activity,
                store = requireComponents.core.store,
                tabsUseCases = requireComponents.useCases.tabsUseCases,
                fragmentStore = store,
                navController = findNavController(),
                settings = requireContext().settings(),
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
                    inlineAutocompleteEditText.setText("")
                },
                dismissDialogAndGoBack = ::dismissDialogAndGoBack,
            ),
        )

        val fromHomeFragment =
            getPreviousDestination()?.destination?.id == R.id.homeFragment

        toolbarView = ToolbarView(
            requireContext(),
            requireContext().settings(),
            requireComponents,
            interactor,
            isPrivate,
            binding.toolbar,
            fromHomeFragment,
        ).also {
            inlineAutocompleteEditText = it.view.findViewById(R.id.mozac_browser_toolbar_edit_url_view)
        }

        val awesomeBar = binding.awesomeBar

        awesomeBarView = AwesomeBarView(
            activity,
            interactor,
            awesomeBar,
            fromHomeFragment,
        )

        binding.awesomeBar.setOnTouchListener { _, _ ->
            binding.root.hideKeyboard()
            false
        }

        awesomeBarView.view.setOnEditSuggestionListener(toolbarView.view::setSearchTerms)

        inlineAutocompleteEditText.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO

        requireComponents.core.engine.speculativeCreateSession(isPrivate)

        // Handle the scenario in which the user selects another search engine before starting a search.
        maybeSelectShortcutEngine(args.searchEngine)

        when (getPreviousDestination()?.destination?.id) {
            R.id.homeFragment -> {
                // When displayed above home, dispatches the touch events to scrim area to the HomeFragment
                binding.searchWrapper.background = ColorDrawable(Color.TRANSPARENT)
                dialog?.window?.decorView?.setOnTouchListener { _, event ->
                    when (event?.action) {
                        MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                            isPrivateButtonClicked = isTouchingPrivateButton(event.x, event.y)
                        }
                        MotionEvent.ACTION_UP -> {
                            if (!isTouchingPrivateButton(
                                    event.x,
                                    event.y,
                                ) && !isPrivateButtonClicked
                            ) {
                                findNavController().popBackStack()
                                isPrivateButtonClicked = false
                            }
                        }
                        else -> isPrivateButtonClicked = false
                    }
                    requireActivity().dispatchTouchEvent(event)
                    false
                }
            }
            R.id.historyFragment -> {
                requireComponents.core.store.state.search.searchEngines.firstOrNull { searchEngine ->
                    searchEngine.id == HISTORY_SEARCH_ENGINE_ID
                }?.let { searchEngine ->
                    store.dispatch(SearchFragmentAction.SearchHistoryEngineSelected(searchEngine))
                }
            }
            R.id.bookmarkFragment -> {
                requireComponents.core.store.state.search.searchEngines.firstOrNull { searchEngine ->
                    searchEngine.id == BOOKMARKS_SEARCH_ENGINE_ID
                }?.let { searchEngine ->
                    store.dispatch(SearchFragmentAction.SearchBookmarksEngineSelected(searchEngine))
                }
            }
            else -> {}
        }

        return binding.root
    }

    @SuppressWarnings("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val showUnifiedSearchFeature = requireContext().settings().showUnifiedSearchFeature

        consumeFlow(requireComponents.core.store) { flow ->
            flow.map { state -> state.search }
                .ifChanged()
                .collect { search ->
                    store.dispatch(
                        SearchFragmentAction.UpdateSearchState(
                            search,
                            showUnifiedSearchFeature,
                        ),
                    )

                    updateSearchSelectorMenu(search.searchEngines)
                }
        }

        setupConstraints(view)

        // When displayed above browser or home screen, dismisses keyboard when touching scrim area
        when (getPreviousDestination()?.destination?.id) {
            R.id.browserFragment, R.id.homeFragment -> {
                binding.searchWrapper.setOnTouchListener { _, _ ->
                    binding.searchWrapper.hideKeyboard()
                    false
                }
            }
            R.id.historyFragment, R.id.bookmarkFragment -> {
                binding.searchWrapper.setOnTouchListener { _, _ ->
                    dismissAllowingStateLoss()
                    true
                }
            }
            else -> {}
        }

        binding.searchEnginesShortcutButton.increaseTapArea(TAP_INCREASE_DPS)
        binding.searchEnginesShortcutButton.isVisible = !showUnifiedSearchFeature

        binding.searchEnginesShortcutButton.setOnClickListener {
            interactor.onSearchShortcutsButtonClicked()
        }

        qrFeature.set(
            createQrFeature(),
            owner = this,
            view = view,
        )

        binding.qrScanButton.isVisible = when {
            showUnifiedSearchFeature -> false
            requireContext().hasCamera() -> true
            else -> false
        }

        binding.qrScanButton.increaseTapArea(TAP_INCREASE_DPS)

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
            Awesomebar.clipboardSuggestionClicked.record(NoExtras())
            val clipboardUrl = requireContext().components.clipboardHandler.extractURL() ?: ""

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                toolbarView.view.edit.updateUrl(clipboardUrl)
                hideClipboardSection()
                inlineAutocompleteEditText.setSelection(clipboardUrl.length)
            } else {
                view.hideKeyboard()
                toolbarView.view.clearFocus()
                (activity as HomeActivity)
                    .openToBrowserAndLoad(
                        searchTermOrURL = clipboardUrl,
                        newTab = store.state.tabId == null,
                        from = BrowserDirection.FromSearchDialog,
                    )
            }
            requireContext().components.clipboardHandler.text = null
        }

        val stubListener = ViewStub.OnInflateListener { _, inflated ->
            val searchSuggestionHintBinding = SearchSuggestionsHintBinding.bind(inflated)

            searchSuggestionHintBinding.learnMore.setOnClickListener {
                (activity as HomeActivity)
                    .openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                            SupportUtils.SumoTopic.SEARCH_SUGGESTION,
                        ),
                        newTab = store.state.tabId == null,
                        from = BrowserDirection.FromSearchDialog,
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

        observeClipboardState()
        observeAwesomeBarState()
        observeShortcutsState()
        observeSuggestionProvidersState()

        consumeFrom(store) {
            updateSearchSuggestionsHintVisibility(it)
            updateToolbarContentDescription(it.searchEngineSource)
            toolbarView.update(it)
            awesomeBarView.update(it)

            if (showUnifiedSearchFeature) {
                addSearchSelector()
                updateQrButton(it)
            }

            updateVoiceSearchButton()
        }
    }

    /**
     * Check whether the search engine identified by [selectedSearchEngineId] is the default search engine
     * and if not update the search state to reflect that a different search engine is currently selected.
     *
     * @param selectedSearchEngineId Id of the search engine currently selected for next searches.
     */
    @VisibleForTesting
    internal fun maybeSelectShortcutEngine(selectedSearchEngineId: String?) {
        if (selectedSearchEngineId == null) return

        val searchState = requireComponents.core.store.state.search
        searchState.searchEngines.firstOrNull {
            it.id == selectedSearchEngineId
        }?.let { selectedSearchEngine ->
            if (selectedSearchEngine != searchState.selectedOrDefaultSearchEngine) {
                interactor.onSearchShortcutEngineSelected(selectedSearchEngine)
            }
        }
    }

    private fun isTouchingPrivateButton(x: Float, y: Float): Boolean {
        val view = parentFragmentManager.primaryNavigationFragment?.view?.findViewInHierarchy {
            it.id == R.id.privateBrowsingButton
        } ?: return false
        return view.getRectWithScreenLocation().contains(x.toInt(), y.toInt())
    }

    private fun hideClipboardSection() {
        binding.fillLinkFromClipboard.isVisible = false
        binding.fillLinkDivider.isVisible = false
        binding.pillWrapperDivider.isVisible = false
        binding.clipboardUrl.isVisible = false
        binding.clipboardTitle.isVisible = false
        binding.linkIcon.isVisible = false
    }

    private fun observeSuggestionProvidersState() = consumeFlow(store) { flow ->
        flow.map { state -> state.toSearchProviderState() }
            .ifChanged()
            .collect { state -> awesomeBarView.updateSuggestionProvidersVisibility(state) }
    }

    private fun observeShortcutsState() = consumeFlow(store) { flow ->
        flow.ifAnyChanged { state -> arrayOf(state.areShortcutsAvailable, state.showSearchShortcuts) }
            .collect { state -> updateSearchShortcutsIcon(state.areShortcutsAvailable, state.showSearchShortcuts) }
    }

    private fun observeAwesomeBarState() = consumeFlow(store) { flow ->
        /*
         * firstUpdate is used to make sure we keep the awesomebar hidden on the first run
         *  of the searchFragmentDialog. We only turn it false after the user has changed the
         *  query as consumeFrom may run several times on fragment start due to state updates.
         * */

        flow.map { state -> state.url != state.query && state.query.isNotBlank() || state.showSearchShortcuts }
            .ifChanged()
            .collect { shouldShowAwesomebar ->
                binding.awesomeBar.visibility = if (shouldShowAwesomebar) {
                    View.VISIBLE
                } else {
                    View.INVISIBLE
                }
            }
    }

    private fun observeClipboardState() = consumeFlow(store) { flow ->
        flow.map { state ->
            val shouldShowView = state.showClipboardSuggestions &&
                state.query.isEmpty() &&
                state.clipboardHasUrl && !state.showSearchShortcuts
            Pair(shouldShowView, state.clipboardHasUrl)
        }
            .ifChanged()
            .collect { (shouldShowView) ->
                updateClipboardSuggestion(shouldShowView)
            }
    }

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

    override fun onResume() {
        super.onResume()

        qrFeature.get()?.let {
            if (it.isScanInProgress) {
                it.scan(binding.searchWrapper.id)
            }
        }

        view?.post {
            // We delay querying the clipboard by posting this code to the main thread message queue,
            // because ClipboardManager will return null if the does app not have input focus yet.
            lifecycleScope.launch(Dispatchers.Cached) {
                val hasUrl = context?.components?.clipboardHandler?.containsURL() ?: false
                store.dispatch(SearchFragmentAction.UpdateClipboardHasUrl(hasUrl))
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
            imm.hideSoftInputFromWindow(view?.windowToken, InputMethodManager.HIDE_IMPLICIT_ONLY)
        }
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        hideDeviceKeyboard()
    }

    override fun onBackPressed(): Boolean {
        return when {
            qrFeature.onBackPressed() -> {
                resetFocus()
                true
            }
            else -> {
                dismissDialogAndGoBack()
                true
            }
        }
    }

    private fun dismissDialogAndGoBack() {
        // In case we're displaying search results, we wouldn't have navigated to home, and
        // so we don't need to navigate "back to" browser fragment.
        // See mirror of this logic in BrowserToolbarController#handleToolbarClick.
        if (store.state.searchTerms.isBlank()) {
            val args by navArgs<SearchDialogFragmentArgs>()
            args.sessionId?.let {
                findNavController().navigate(
                    SearchDialogFragmentDirections.actionGlobalBrowser(null),
                )
            }
        }

        view?.hideKeyboard()
        dismissAllowingStateLoss()
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
                val normalizedUrl = result.toNormalizedUrl()
                if (!normalizedUrl.toUri().isHttpOrHttps) {
                    activity?.let {
                        AlertDialog.Builder(it).apply {
                            setMessage(R.string.qr_scanner_dialog_invalid)
                            setPositiveButton(R.string.qr_scanner_dialog_invalid_ok) { dialog: DialogInterface, _ ->
                                dialog.dismiss()
                            }
                            create()
                        }.show()
                    }
                } else {
                    binding.qrScanButton.isChecked = false
                    activity?.let {
                        AlertDialog.Builder(it).apply {
                            val spannable = resources.getSpanned(
                                R.string.qr_scanner_confirmation_dialog_message,
                                getString(R.string.app_name) to StyleSpan(Typeface.BOLD),
                                normalizedUrl to StyleSpan(Typeface.ITALIC),
                            )
                            setMessage(spannable)
                            setNegativeButton(R.string.qr_scanner_dialog_negative) { dialog: DialogInterface, _ ->
                                dialog.cancel()
                            }
                            setPositiveButton(R.string.qr_scanner_dialog_positive) { dialog: DialogInterface, _ ->
                                (activity as? HomeActivity)?.openToBrowserAndLoad(
                                    searchTermOrURL = normalizedUrl,
                                    newTab = store.state.tabId == null,
                                    from = BrowserDirection.FromSearchDialog,
                                    flags = EngineSession.LoadUrlFlags.external(),
                                )
                                dialog.dismiss()
                            }
                            create()
                        }.show()
                    }
                }
            },
        )
    }

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19920
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray,
    ) {
        when (requestCode) {
            REQUEST_CODE_CAMERA_PERMISSIONS -> qrFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
                if (grantResults.contains(PackageManager.PERMISSION_DENIED)) {
                    resetFocus()
                }
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

    /**
     * Updates the search selector menu with the given list of available search engines.
     *
     * @param searchEngines List of [SearchEngine] to display.
     */
    private fun updateSearchSelectorMenu(searchEngines: List<SearchEngine>) {
        val searchEngineList = searchEngines
            .map {
                TextMenuCandidate(
                    text = it.name,
                    start = DrawableMenuIcon(
                        drawable = it.icon.toDrawable(resources),
                        tint = if (it.type == SearchEngine.Type.APPLICATION) {
                            requireContext().getColorFromAttr(R.attr.textPrimary)
                        } else {
                            null
                        },
                    ),
                ) {
                    interactor.onMenuItemTapped(SearchSelectorMenu.Item.SearchEngine(it))
                }
            }

        searchSelectorMenu.menuController.submitList(searchSelectorMenu.menuItems(searchEngineList))
        toolbarView.view.invalidateActions()
    }

    private fun addSearchSelector() {
        if (searchSelectorAlreadyAdded) return

        toolbarView.view.addEditActionStart(
            SearchSelectorToolbarAction(
                store = store,
                defaultSearchEngine = requireComponents.core.store.state.search.selectedOrDefaultSearchEngine,
                menu = searchSelectorMenu,
            ),
        )

        searchSelectorAlreadyAdded = true
    }

    private fun updateVoiceSearchButton() {
        when (isSpeechAvailable() && requireContext().settings().shouldShowVoiceSearch) {
            true -> {
                if (voiceSearchButtonAction == null) {
                    voiceSearchButtonAction = IncreasedTapAreaActionDecorator(
                        BrowserToolbar.Button(
                            AppCompatResources.getDrawable(requireContext(), R.drawable.ic_microphone)!!,
                            requireContext().getString(R.string.voice_search_content_description),
                            visible = { true },
                            listener = ::launchVoiceSearch,
                        ),
                    ).also { action ->
                        toolbarView.view.run {
                            addEditActionEnd(action)
                            invalidateActions()
                        }
                    }
                }
            }
            false -> {
                voiceSearchButtonAction?.let { action ->
                    toolbarView.view.removeEditActionEnd(action)
                    voiceSearchButtonAction = null
                }
            }
        }
    }

    private fun launchVoiceSearch() {
        // Note if a user disables speech while the app is on the search fragment
        // the voice button will still be available and *will* cause a crash if tapped,
        // since the `visible` call is only checked on create. In order to avoid extra complexity
        // around such a small edge case, we make the button have no functionality in this case.
        if (!isSpeechAvailable()) { return }

        VoiceSearch.tapped.record(NoExtras())
        speechIntent.apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_PROMPT, requireContext().getString(R.string.voice_search_explainer))
        }

        startForResult.launch(speechIntent)
    }

    private fun updateQrButton(searchFragmentState: SearchFragmentState) {
        val searchEngine = searchFragmentState.searchEngineSource.searchEngine
        when (
            searchEngine?.isGeneral == true || searchEngine?.type == SearchEngine.Type.CUSTOM
        ) {
            true -> {
                if (qrButtonAction == null) {
                    qrButtonAction = IncreasedTapAreaActionDecorator(
                        BrowserToolbar.Button(
                            AppCompatResources.getDrawable(requireContext(), R.drawable.ic_qr)!!,
                            requireContext().getString(R.string.search_scan_button),
                            autoHide = { true },
                            listener = ::launchQr,
                        ),
                    ).also { action ->
                        toolbarView.view.run {
                            addEditActionEnd(action)
                            invalidateActions()
                        }
                    }
                }
            }
            false -> {
                qrButtonAction?.let { action ->
                    toolbarView.view.removeEditActionEnd(action)
                    qrButtonAction = null
                }
            }
        }
    }

    private fun launchQr() {
        if (!requireContext().hasCamera()) {
            return
        }

        view?.hideKeyboard()
        toolbarView.view.clearFocus()

        when {
            requireContext().settings().shouldShowCameraPermissionPrompt ->
                qrFeature.get()?.scan(binding.searchWrapper.id)
            requireContext().isPermissionGranted(Manifest.permission.CAMERA) ->
                qrFeature.get()?.scan(binding.searchWrapper.id)
            else -> {
                interactor.onCameraPermissionsNeeded()
                resetFocus()
                view?.hideKeyboard()
                toolbarView.view.requestFocus()
            }
        }

        requireContext().settings().setCameraPermissionNeededState = false
    }

    private fun isSpeechAvailable(): Boolean = speechIntent.resolveActivity(requireContext().packageManager) != null

    private fun updateClipboardSuggestion(
        shouldShowView: Boolean,
    ) {
        binding.fillLinkFromClipboard.isVisible = shouldShowView
        binding.fillLinkDivider.isVisible = shouldShowView
        binding.pillWrapperDivider.isVisible =
            !(shouldShowView && requireComponents.settings.shouldUseBottomToolbar)
        binding.clipboardTitle.isVisible = shouldShowView
        binding.linkIcon.isVisible = shouldShowView

        if (shouldShowView) {
            val contentDescription = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                "${binding.clipboardTitle.text}."
            } else {
                val clipboardUrl = context?.components?.clipboardHandler?.extractURL()

                if (clipboardUrl != null && !((activity as HomeActivity).browsingModeManager.mode.isPrivate)) {
                    requireComponents.core.engine.speculativeConnect(clipboardUrl)
                }
                binding.clipboardUrl.text = clipboardUrl
                binding.clipboardUrl.isVisible = shouldShowView
                "${binding.clipboardTitle.text}, ${binding.clipboardUrl.text}."
            }

            binding.fillLinkFromClipboard.contentDescription = contentDescription
        }
    }

    private fun updateToolbarContentDescription(source: SearchEngineSource) {
        source.searchEngine?.let { engine ->
            toolbarView.view.contentDescription = engine.name + ", " + inlineAutocompleteEditText.hint
        }

        inlineAutocompleteEditText.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
    }

    private fun updateSearchShortcutsIcon(
        areShortcutsAvailable: Boolean,
        showShortcuts: Boolean,
    ) {
        val showUnifiedSearchFeature = requireContext().settings().showUnifiedSearchFeature

        view?.apply {
            binding.searchEnginesShortcutButton.isVisible =
                !showUnifiedSearchFeature && areShortcutsAvailable
            binding.pillWrapper.isVisible = !showUnifiedSearchFeature
            binding.searchEnginesShortcutButton.isChecked = showShortcuts

            val color = if (showShortcuts) R.attr.textOnColorPrimary else R.attr.textPrimary
            binding.searchEnginesShortcutButton.compoundDrawables[0]?.setTint(
                requireContext().getColorFromAttr(color),
            )
        }
    }

    /**
     * Gets the previous visible [NavBackStackEntry].
     * This skips over any [NavBackStackEntry] that is associated with a [NavGraph] or refers to this
     * class as a navigation destination.
     */
    @VisibleForTesting
    @SuppressLint("RestrictedApi")
    internal fun getPreviousDestination(): NavBackStackEntry? {
        // This duplicates the platform functionality for "previousBackStackEntry" but additionally skips this entry.

        val descendingEntries = findNavController().backQueue.reversed().iterator()
        // Throw the topmost destination away.
        if (descendingEntries.hasNext()) {
            descendingEntries.next()
        }

        while (descendingEntries.hasNext()) {
            val entry = descendingEntries.next()
            // Using the canonicalName is safer - see https://github.com/mozilla-mobile/android-components/pull/10810
            // simpleName is used as a backup to avoid the not null assertion (!!) operator.
            val currentClassName = this::class.java.canonicalName?.substringAfterLast('.')
                ?: this::class.java.simpleName

            // Throw this entry away if it's the current top and ignore returning the base nav graph.
            if (entry.destination !is NavGraph && !entry.destination.displayName.contains(currentClassName, true)) {
                return entry
            }
        }
        return null
    }

    companion object {
        private const val TAP_INCREASE_DPS = 8
        private const val QR_FRAGMENT_TAG = "MOZAC_QR_FRAGMENT"
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
    }
}
