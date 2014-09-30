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

    public static final String ABOUT_BLANK = "about:blank";

    // TODO: Localize this with region.properties (or a similar solution). See bug 1065306.
    public static final String DEFAULT_ENGINE_IDENTIFIER = "yahoo";

    public static final String PREF_SEARCH_ENGINE_KEY = "search.engines.default";
}
