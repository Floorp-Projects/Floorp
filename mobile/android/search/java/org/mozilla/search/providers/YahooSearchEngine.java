/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.providers;

import android.net.Uri;

import org.mozilla.search.Constants;

public class YahooSearchEngine extends SearchEngine {

    private static final String HIDE_BANNER_CSS = "#nav,#header{display:none}";

    private static final Uri RESULTS_URI = Uri.parse("https://search.yahoo.com/search");
    private static final String RESULTS_URI_QUERY_PARAM = "p";

    private static final Uri SUGGEST_URI = Uri.parse(
            "https://search.yahoo.com/sugg/ff?output=fxjson&appid=ffm&nresults=" + Constants.SUGGESTION_MAX);
    private static final String SUGGEST_URI_QUERY_PARAM = "command";

    @Override
    public String getInjectableCss() {
        return HIDE_BANNER_CSS;
    }

    @Override
    protected Uri getResultsUri() {
        return RESULTS_URI;
    }

    @Override
    protected Uri getSuggestionUri() {
        return SUGGEST_URI;
    }

    @Override
    protected String getSuggestionQueryParam() {
        return SUGGEST_URI_QUERY_PARAM;
    }

    @Override
    protected String getResultsQueryParam() {
        return RESULTS_URI_QUERY_PARAM;
    }

}
