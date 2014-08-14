/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

/**
 * Key should not be stored here. For more info on storing keys, see
 * https://github.com/ericedens/FirefoxSearch/issues/3
 */
public class Constants {

    public static final String POSTSEARCH_FRAGMENT = "org.mozilla.search.POSTSEARCH_FRAGMENT";
    public static final String PRESEARCH_FRAGMENT = "org.mozilla.search.PRESEARCH_FRAGMENT";
    public static final String SEARCH_FRAGMENT = "org.mozilla.search.SEARCH_FRAGMENT";

    public static final int SUGGESTION_MAX = 5;

    public static final String ABOUT_BLANK = "about:blank";

    // The default search engine for new users. This should match one of
    // the SearchEngineFactory.Engine enum values.
    public static final String DEFAULT_SEARCH_ENGINE = "YAHOO";
}
