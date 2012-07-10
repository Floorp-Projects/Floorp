/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

public final class StringUtils {

    private static final String HTTP_PREFIX = "http://";
    private static final String PROTOCOL_DELIMITER = "://";

    /**
     * Gets a display-friendly URL.
     * If the URL starts with "http://", this prefix is removed. If the URL
     * contains exactly one "/", and it's at the end of the URL, it is removed.
     * Examples:
     *   - http://www.google.com/ -> www.google.com
     *   - http://mozilla.org/en-US/ -> mozilla.org/en-US/
     *   - https://www.google.com/ -> https://www.google.com
     */
    public static String prettyURL(String url) {
        if (url == null)
            return url;

        // remove http:// prefix from URLs
        if (url.startsWith(HTTP_PREFIX))
            url = url.substring(HTTP_PREFIX.length());

        // remove trailing / on root URLs
        int delimiterIndex = url.indexOf(PROTOCOL_DELIMITER);
        int fromIndex = (delimiterIndex == -1 ? 0 : delimiterIndex + PROTOCOL_DELIMITER.length());
        int firstSlash = url.indexOf('/', fromIndex);
        if (firstSlash == url.length() - 1)
            url = url.substring(0, firstSlash);

        return url;
    }

}
