/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.graphics.Rect;

/**
 * Allows fragments to pass a search event to the main activity.
 */
public interface AcceptsSearchQuery {

    /**
     * Shows search suggestions.
     * @param query
     */
    void onSuggest(String query);

    /**
     * Starts a search.
     *
     * @param query
     */
    void onSearch(String query);

    /**
     * Starts a search and animates a suggestion.
     *
     * @param query
     * @param suggestionAnimation
     */
    void onSearch(String query, SuggestionAnimation suggestionAnimation);

    /**
     * Handles a change to the current search query.
     *
     * @param query
     */
    void onQueryChange(String query);

    /**
     * Interface to specify search suggestion animation details.
     */
    public interface SuggestionAnimation {
        public Rect getStartBounds();
    }
}
