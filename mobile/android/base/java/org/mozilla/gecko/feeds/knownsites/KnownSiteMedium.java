/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.knownsites;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Medium.com
 */
public class KnownSiteMedium implements KnownSite {
    @Override
    public String getURLSearchString() {
        return "://medium.com/";
    }

    @Override
    public String getFeedFromURL(String url) {
        Pattern pattern = Pattern.compile("https?://medium.com/([^/]+)(/.*)?");
        Matcher matcher = pattern.matcher(url);
        if (matcher.matches()) {
            return String.format("https://medium.com/feed/%s", matcher.group(1));
        }
        return null;
    }
}
