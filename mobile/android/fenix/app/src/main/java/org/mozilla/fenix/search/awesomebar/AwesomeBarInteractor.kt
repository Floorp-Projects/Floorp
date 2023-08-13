/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags

/**
 * Interface for the AwesomeBarView Interactor. This interface is implemented by objects that want
 * to respond to user interaction on the AwesomebarView
 */
interface AwesomeBarInteractor {

    /**
     * Called whenever a suggestion containing a URL is tapped
     * @param url the url the suggestion was providing
     * @param flags the [LoadUrlFlags] to use when loading the provided url.
     */
    fun onUrlTapped(url: String, flags: LoadUrlFlags = LoadUrlFlags.none())

    /**
     * Called whenever a search engine suggestion is tapped
     * @param searchTerms the query contained by the search suggestion
     */
    fun onSearchTermsTapped(searchTerms: String)

    /**
     * Called whenever a suggestion for a previously used search term is tapped.
     * @param searchTerms the query contained by the search suggestion.
     */
    fun onHistorySearchTermTapped(searchTerms: String)

    /**
     * Called whenever a search engine shortcut is tapped
     * @param searchEngine the searchEngine that was selected
     */
    fun onSearchShortcutEngineSelected(searchEngine: SearchEngine)

    /**
     * Called whenever the "Search Engine Settings" item is tapped
     */
    fun onClickSearchEngineSettings()

    /**
     * Called whenever an existing session is selected from the sessionSuggestionProvider
     */
    fun onExistingSessionSelected(tabId: String)

    /**
     * Called whenever the Shortcuts button is clicked
     */
    fun onSearchShortcutsButtonClicked()

    /**
     * Called whenever search engine suggestion is tapped
     */
    fun onSearchEngineSuggestionSelected(searchEngine: SearchEngine)
}
