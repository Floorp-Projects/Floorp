/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import android.content.Context;
import android.util.Log;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.util.GeckoJarReader;

/**
 * Loader implementation for loading icons from the omni.ja (jar:jar: URLs).
 *
 * https://developer.mozilla.org/en-US/docs/Mozilla/About_omni.ja_(formerly_omni.jar)
 */
public class JarLoader implements IconLoader {
    private static final String LOGTAG = "Gecko/JarLoader";

    @Override
    public IconResponse load(IconRequest request) {
        if (request.shouldSkipDisk()) {
            return null;
        }

        final String iconUrl = request.getBestIcon().getUrl();

        if (!iconUrl.startsWith("jar:jar:")) {
            return null;
        }

        try {
            final Context context = request.getContext();
            return IconResponse.create(
                    GeckoJarReader.getBitmap(context, context.getResources(), iconUrl));
        } catch (Exception e) {
            // Just about anything could happen here.
            Log.w(LOGTAG, "Error fetching favicon from JAR.", e);
            return null;
        }
    }
}
