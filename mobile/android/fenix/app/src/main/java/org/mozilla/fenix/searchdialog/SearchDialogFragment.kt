/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.searchdialog

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.constraintlayout.widget.ConstraintProperties.BOTTOM
import androidx.constraintlayout.widget.ConstraintProperties.PARENT_ID
import androidx.constraintlayout.widget.ConstraintProperties.TOP
import androidx.constraintlayout.widget.ConstraintSet
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_search.view.*
import kotlinx.android.synthetic.main.fragment_search_dialog.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.selector.findTab
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.SearchFragmentState
import org.mozilla.fenix.search.SearchFragmentStore
import org.mozilla.fenix.search.SearchInteractor
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.toolbar.ToolbarView
import org.mozilla.fenix.utils.Settings

typealias SearchDialogFragmentStore = SearchFragmentStore
typealias SearchDialogInteractor = SearchInteractor
fun Settings.shouldShowSearchSuggestions(isPrivate: Boolean): Boolean {
    return if (isPrivate) {
        shouldShowSearchSuggestions && shouldShowSearchSuggestionsInPrivate
    } else {
        shouldShowSearchSuggestions
    }
}

class SearchDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {

    private lateinit var interactor: SearchDialogInteractor
    private lateinit var store: SearchDialogFragmentStore
    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView

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
        val view = inflater.inflate(R.layout.fragment_search_dialog, container, false)
        store = SearchDialogFragmentStore(setUpState())

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
        )

        awesomeBarView = AwesomeBarView(
            requireContext(),
            interactor,
            view.awesomeBar
        )

        return view
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        if (view.context.settings().toolbarPosition == ToolbarPosition.BOTTOM) {
            ConstraintSet().apply {
                clone(search_wrapper)

                clear(toolbar.id, TOP)
                connect(toolbar.id, BOTTOM, PARENT_ID, BOTTOM)

                clear(awesomeBar.id, TOP)
                clear(awesomeBar.id, BOTTOM)
                connect(awesomeBar.id, TOP, PARENT_ID, TOP)
                connect(awesomeBar.id, BOTTOM, toolbar.id, TOP)

                applyTo(search_wrapper)
            }
        }

        search_wrapper.setOnClickListener {
            it.hideKeyboard()
            dismissAllowingStateLoss()
        }

        consumeFrom(store) {
            awesomeBar?.visibility = if (it.query.isEmpty()) View.INVISIBLE else View.VISIBLE
            toolbarView.update(it)
            awesomeBarView.update(it)
        }
    }

    override fun onBackPressed(): Boolean {
        view?.hideKeyboard()
        dismissAllowingStateLoss()

        return true
    }

    private fun setUpState(): SearchFragmentState {
        val activity = activity as HomeActivity
        val settings = activity.settings()
        val args by navArgs<SearchDialogFragmentArgs>()
        val tabId = args.sessionId
        val tab = tabId?.let { requireComponents.core.store.state.findTab(it) }
        val url = tab?.content?.url.orEmpty()
        val currentSearchEngine = SearchEngineSource.Default(
            requireComponents.search.provider.getDefaultEngine(requireContext())
        )
        val isPrivate = activity.browsingModeManager.mode.isPrivate
        val areShortcutsAvailable =
            requireContext().components.search.provider.installedSearchEngines(requireContext())
                .list.size >= MINIMUM_SEARCH_ENGINES_NUMBER_TO_SHOW_SHORTCUTS
        return SearchFragmentState(
            query = url,
            url = url,
            searchTerms = tab?.content?.searchTerms.orEmpty(),
            searchEngineSource = currentSearchEngine,
            defaultEngineSource = currentSearchEngine,
            showSearchSuggestions = settings.shouldShowSearchSuggestions(isPrivate),
            showSearchSuggestionsHint = false,
            showSearchShortcuts = settings.shouldShowSearchShortcuts &&
                    url.isEmpty() &&
                    areShortcutsAvailable,
            areShortcutsAvailable = areShortcutsAvailable,
            showClipboardSuggestions = settings.shouldShowClipboardSuggestions,
            showHistorySuggestions = settings.shouldShowHistorySuggestions,
            showBookmarkSuggestions = settings.shouldShowBookmarkSuggestions,
            tabId = tabId,
            pastedText = args.pastedText,
            searchAccessPoint = args.searchAccessPoint
        )
    }

    companion object {
        private const val MINIMUM_SEARCH_ENGINES_NUMBER_TO_SHOW_SHORTCUTS = 2
    }
}
