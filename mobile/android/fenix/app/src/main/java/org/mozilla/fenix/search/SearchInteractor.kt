/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import org.mozilla.fenix.search.awesomebar.AwesomeBarInteractor
import org.mozilla.fenix.search.toolbar.ToolbarInteractor

/**
 * Interactor for the search screen
 * Provides implementations for the AwesomeBarView and ToolbarView
 */
class SearchInteractor(
    private val searchController: SearchController
) : AwesomeBarInteractor, ToolbarInteractor {

    override fun onUrlCommitted(url: String) {
        searchController.handleUrlCommitted(url)
    }

    override fun onEditingCanceled() {
        searchController.handleEditingCancelled()
    }

    override fun onTextChanged(text: String) {
        searchController.handleTextChanged(text)
    }

    override fun onUrlTapped(url: String) {
        searchController.handleUrlTapped(url)
    }

    override fun onSearchTermsTapped(searchTerms: String) {
        searchController.handleSearchTermsTapped(searchTerms)
    }

    override fun onSearchShortcutEngineSelected(searchEngine: SearchEngine) {
        searchController.handleSearchShortcutEngineSelected(searchEngine)
    }

    override fun onClickSearchEngineSettings() {
        searchController.handleClickSearchEngineSettings()
    }

    fun turnOnStartedTyping() {
        searchController.handleTurnOnStartedTyping()
    }

    override fun onExistingSessionSelected(session: Session) {
        searchController.handleExistingSessionSelected(session)
    }
}
