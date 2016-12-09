/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.net.Uri;
import android.text.TextUtils;

public class UrlUtils {
    public static String normalize(String input) {
        Uri uri = Uri.parse(input);

        if (TextUtils.isEmpty(uri.getScheme())) {
            uri = uri.buildUpon().scheme("http").build();
        }

        return uri.toString();
    }
}
