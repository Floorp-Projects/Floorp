/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.searchdialog

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import kotlinx.android.synthetic.main.fragment_search.view.*
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.logDebug
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.SearchFragmentState
import org.mozilla.fenix.search.awesomebar.AwesomeBarInteractor
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.toolbar.ToolbarInteractor
import org.mozilla.fenix.search.toolbar.ToolbarView

class TempSearchInteractor(val onTextChangedCallback: (String) -> Unit) : ToolbarInteractor, AwesomeBarInteractor {
    override fun onUrlCommitted(url: String) {
        logDebug("boek", "onUrlCommitted $url")
    }

    override fun onEditingCanceled() {
        logDebug("boek", "onEditingCanceled")
    }

    override fun onTextChanged(text: String) {
        onTextChangedCallback.invoke(text)
    }

    override fun onUrlTapped(url: String) {
        logDebug("boek", "onEditingCanceled")
    }

    override fun onSearchTermsTapped(searchTerms: String) {
        logDebug("boek", "onEditingCanceled")
    }

    override fun onSearchShortcutEngineSelected(searchEngine: SearchEngine) {
        logDebug("boek", "onEditingCanceled")
    }

    override fun onClickSearchEngineSettings() {
        logDebug("boek", "onEditingCanceled")
    }

    override fun onExistingSessionSelected(session: Session) {
        logDebug("boek", "onEditingCanceled")
    }

    override fun onExistingSessionSelected(tabId: String) {
        logDebug("boek", "onEditingCanceled")
    }

    override fun onSearchShortcutsButtonClicked() {
        logDebug("boek", "onEditingCanceled")
    }
}

class SearchDialogFragment : AppCompatDialogFragment() {

    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView
    private val tempInteractor = TempSearchInteractor {
        view?.awesomeBar?.visibility = if (it.isEmpty()) View.INVISIBLE else View.VISIBLE

        awesomeBarView.update(
            SearchFragmentState(
                query = it,
                url = "",
                searchTerms = "",
                searchEngineSource = SearchEngineSource.Default(requireComponents.search.provider.getDefaultEngine(requireContext())),
                defaultEngineSource = SearchEngineSource.Default(requireComponents.search.provider.getDefaultEngine(requireContext())),
                showSearchSuggestions = true,
                showSearchSuggestionsHint = false,
                showSearchShortcuts = false,
                areShortcutsAvailable = false,
                showClipboardSuggestions = true,
                showHistorySuggestions = true,
                showBookmarkSuggestions = true,
                tabId = null,
                pastedText = null,
                searchAccessPoint = null
            )
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.SearchDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_search_dialog, container, false)

        toolbarView = ToolbarView(
            requireContext(),
            tempInteractor,
            null,
            false,
            view.toolbar,
            requireComponents.core.engine
        )

        awesomeBarView = AwesomeBarView(
            requireContext(),
            tempInteractor,
            view.awesomeBar
        )

        return view
    }
}
