/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.Context;

import java.util.Arrays;
import java.util.List;

/**
 * TODO: Quick hack with hardcoded values. We need to import and read search configuration files. (#184)
 */
public class SearchEngineManager {
    public static List<SearchEngine> getSearchEngines(Context context) {
        return Arrays.asList(
            new SearchEngine(0, "Yahoo"),
            new SearchEngine(0, "Google"),
            new SearchEngine(0, "Amazon"),
            new SearchEngine(0, "DuckDuckGo"),
            new SearchEngine(0, "Twitter"),
            new SearchEngine(0, "Wikipedia")
        );
    }

    public static SearchEngine getDefaultSearchEngine(Context context) {
        return getSearchEngines(context).get(0);
    }
}
