/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.knownsites;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

/**
 * A site we know and for which we can guess the feed URL from an arbitrary URL.
 */
public interface KnownSite {
    /**
     * Get a search string to find URLs of this site in our database. This search string is usually
     * a partial domain / URL.
     *
     * For example we could return "medium.com" to find all URLs that contain this string. This could
     * obviously find URLs that are not actually medium.com sites. This is acceptable as long as
     * getFeedFromURL() can handle these inputs and either returns a feed for valid URLs or null for
     * other matches that are not related to this site.
     */
    @NonNull String getURLSearchString();

    /**
     * Get the Feed URL for this URL. For a known site we can "guess" the feed URL from an URL
     * pointing to any page. The input URL will be a result from the database found with the value
     * returned by getURLSearchString().
     *
     * Example:
     * - Input:  https://medium.com/@antlam/ux-thoughts-for-2016-1fc1d6e515e8
     * - Output: https://medium.com/feed/@antlam
     *
     * @return the url representing a feed, or null if a feed could not be determined.
     */
    @Nullable String getFeedFromURL(String url);
}
