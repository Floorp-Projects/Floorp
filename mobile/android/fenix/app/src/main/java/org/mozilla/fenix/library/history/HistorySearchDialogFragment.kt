/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewStub
import android.view.accessibility.AccessibilityEvent
import android.view.inputmethod.InputMethodManager
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.constraintlayout.widget.ConstraintProperties.BOTTOM
import androidx.constraintlayout.widget.ConstraintProperties.PARENT_ID
import androidx.constraintlayout.widget.ConstraintProperties.TOP
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.view.isVisible
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.FragmentHistorySearchDialogBinding
import org.mozilla.fenix.databinding.SearchSuggestionsHintBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.library.history.awesomebar.AwesomeBarView
import org.mozilla.fenix.library.history.toolbar.ToolbarView
import org.mozilla.fenix.settings.SupportUtils

@Suppress("TooManyFunctions")
class HistorySearchDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {
    private var _binding: FragmentHistorySearchDialogBinding? = null
    private val binding get() = _binding!!

    private lateinit var interactor: HistorySearchDialogInteractor
    private lateinit var store: HistorySearchFragmentStore
    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView

    private var dialogHandledAction = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.SearchDialogStyle)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return object : Dialog(requireContext(), this.theme) {
            override fun onBackPressed() {
                this@HistorySearchDialogFragment.onBackPressed()
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentHistorySearchDialogBinding.inflate(inflater, container, false)
        val activity = requireActivity() as HomeActivity

        store = HistorySearchFragmentStore(
            createInitialHistorySearchFragmentState()
        )

        interactor = HistorySearchDialogInteractor(
            HistorySearchDialogController(
                activity = activity,
                metrics = activity.components.analytics.metrics,
                fragmentStore = store,
                clearToolbarFocus = {
                    dialogHandledAction = true
                    toolbarView.view.hideKeyboard()
                    toolbarView.view.clearFocus()
                },
            )
        )

        toolbarView = ToolbarView(
            context = requireContext(),
            interactor = interactor,
            isPrivate = false,
            view = binding.toolbar,
        )

        val awesomeBar = binding.awesomeBar

        awesomeBarView = AwesomeBarView(
            activity,
            interactor,
            awesomeBar,
        )

        awesomeBarView.view.setOnEditSuggestionListener(toolbarView.view::setSearchTerms)

        return binding.root
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupConstraints(view)

        binding.searchWrapper.setOnTouchListener { _, _ ->
            dismissAllowingStateLoss()
            true
        }
        val stubListener = ViewStub.OnInflateListener { _, inflated ->
            val searchSuggestionHintBinding = SearchSuggestionsHintBinding.bind(inflated)

            searchSuggestionHintBinding.learnMore.setOnClickListener {
                (activity as HomeActivity)
                    .openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                            SupportUtils.SumoTopic.SEARCH_SUGGESTION
                        ),
                        newTab = true,
                        from = BrowserDirection.FromHistorySearchDialog
                    )
            }

            searchSuggestionHintBinding.allow.setOnClickListener {
                inflated.visibility = View.GONE
                requireContext().settings().also {
                    it.shouldShowSearchSuggestionsInPrivate = true
                    it.showSearchSuggestionsInPrivateOnboardingFinished = true
                }
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

        binding.searchSuggestionsHintDivider.isVisible = false
        binding.searchSuggestionsHint.isVisible = false
        binding.searchSuggestionsHint.setOnInflateListener((stubListener))
        if (view.context.settings().accessibilityServicesEnabled) {
            updateAccessibilityTraversalOrder()
        }

        observeAwesomeBarState()

        consumeFrom(store) {
            toolbarView.update(it)
            awesomeBarView.update(it)
        }
    }

    private fun observeAwesomeBarState() = consumeFlow(store) { flow ->
        flow.map { state -> state.query.isNotBlank() }
            .ifChanged()
            .collect { shouldShowAwesomebar ->
                binding.awesomeBar.visibility = if (shouldShowAwesomebar) {
                    View.VISIBLE
                } else {
                    View.INVISIBLE
                }
            }
    }

    private fun updateAccessibilityTraversalOrder() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP_MR1) {
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
     * is also dismissing.
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
        view?.hideKeyboard()
        dismissAllowingStateLoss()

        return true
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

                applyTo(binding.searchWrapper)
            }
        }
    }
}
