/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.providers;

import android.net.Uri;

/**
 * Extend this class to add a new search engine to
 * the search activity.
 */
public abstract class SearchEngine {

    // Boilerplate bookmarklet-style JS for injecting CSS into the
    // head of a web page. The actual CSS is inserted at `%s`.
    private static final String STYLE_INJECTION_SCRIPT =
            "javascript:(function(){" +
                    "var tag=document.createElement('style');" +
                    "tag.type='text/css';" +
                    "document.getElementsByTagName('head')[0].appendChild(tag);" +
                    "tag.innerText='%s'})();";

    private String suggestionTemplate;

    /**
     * Retrieve a JS snippet, in bookmarklet style, that can be used
     * to modify the results page.
     */
    public String getInjectableJs() {
        return String.format(STYLE_INJECTION_SCRIPT, getInjectableCss());
    }

    /**
     * Determine whether a particular url belongs to this search engine. If not,
     * the url will be sent to Fennec.
     */
    final public boolean isSearchResultsPage(String url) {
        return getResultsUri().getAuthority().equalsIgnoreCase(Uri.parse(url).getAuthority());
    }

    /**
     * Create a uri string that can be used to fetch suggestions.
     *
     * @param query The user's partial query. This method will handle url escaping.
     */
    final public String suggestUriForQuery(String query) {
        return getSuggestionUri().buildUpon().appendQueryParameter(getSuggestionQueryParam(), query).build().toString();
    }

    /**
     * Create a uri strung that can be used to fetch the results page.
     *
     * @param query The user's query. This method will handle url escaping.
     */
    final public String resultsUriForQuery(String query) {
        return getResultsUri().buildUpon().appendQueryParameter(getResultsQueryParam(), query).build().toString();
    }

    /**
     * Create a suggestion uri that can be used by SuggestClient
     */
    final public String getSuggestionTemplate(String placeholder) {
        if (suggestionTemplate == null) {
            suggestionTemplate = suggestUriForQuery(placeholder);
        }
        return suggestionTemplate;
    }

    /**
     * Retrieve a snippet of CSS that can be used to modify the appearance
     * of the search results page. Currently this is used to hide
     * the web site's search bar and facet bar.
     */
    protected abstract String getInjectableCss();

    /**
     * Retrieve the base Uri that should be used when retrieving
     * the results page. This may include params that do not vary --
     * for example, the user's locale.
     */
    protected abstract Uri getResultsUri();

    /**
     * Retrieve the base Uri that should be used when fetching
     * suggestions. This may include params that do not vary --
     * for example, the user's locale.
     */
    protected abstract Uri getSuggestionUri();

    /**
     * Retrieve the uri query param for the user's partial query.
     * Used for suggestions.
     */
    protected abstract String getSuggestionQueryParam();

    /**
     * Retrieve the uri query param that holds the user's final query.
     * Used for results.
     */
    protected abstract String getResultsQueryParam();
}
