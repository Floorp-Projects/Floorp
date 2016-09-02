/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons;

import android.content.Context;
import android.support.annotation.CheckResult;

/**
 * Entry point for loading icons for websites (just high quality icons, can be favicons or
 * touch icons).
 *
 * The API is loosely inspired by Picasso's builder.
 *
 * Example:
 *
 *     Icons.with(context)
 *         .pageUrl(pageURL)
 *         .skipNetwork()
 *         .privileged(true)
 *         .icon(IconDescriptor.createGenericIcon(url))
 *         .build()
 *         .execute(callback);
 */
public abstract class Icons {
    /**
     * Create a new request for loading a website icon.
     */
    @CheckResult
    public static IconRequestBuilder with(Context context) {
        return new IconRequestBuilder(context);
    }
}
