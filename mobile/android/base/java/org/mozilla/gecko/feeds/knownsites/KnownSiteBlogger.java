/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.knownsites;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Blogger.com
 */
public class KnownSiteBlogger implements KnownSite {
    @Override
    public String getURLSearchString() {
        return ".blogspot.com";
    }

    @Override
    public String getFeedFromURL(String url) {
        Pattern pattern = Pattern.compile("https?://(www\\.)?(.*?)\\.blogspot\\.com(/.*)?");
        Matcher matcher = pattern.matcher(url);
        if (matcher.matches()) {
            return String.format("https://%s.blogspot.com/feeds/posts/default", matcher.group(2));
        }
        return null;
    }
}
