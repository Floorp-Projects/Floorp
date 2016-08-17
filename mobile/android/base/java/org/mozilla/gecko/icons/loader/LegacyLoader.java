/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import android.content.ContentResolver;
import android.content.Context;
import android.graphics.Bitmap;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

/**
 * This legacy loader loads icons from the abandoned database storage. This loader should only exist
 * for a couple of releases and be removed afterwards.
 *
 * When updating to an app version with the new loaders our initial storage won't have any data so
 * we need to continue loading from the database storage until the new storage has a good set of data.
 */
public class LegacyLoader implements IconLoader {
    @Override
    public IconResponse load(IconRequest request) {
        if (request.shouldSkipDisk()) {
            return null;
        }

        final Bitmap bitmap = loadBitmapFromDatabase(request);

        if (bitmap == null) {
            return null;
        }

        return IconResponse.create(bitmap);
    }

    /* package-private */ Bitmap loadBitmapFromDatabase(IconRequest request) {
        final Context context = request.getContext();
        final ContentResolver contentResolver = context.getContentResolver();
        final BrowserDB db = GeckoProfile.get(request.getContext()).getDB();

        // We ask the database for the favicon URL and ignore the icon URL in the request object:
        // As we are not updating the database anymore the icon might be stored under a different URL.
        final String legacyFaviconUrl = db.getFaviconURLFromPageURL(contentResolver, request.getPageUrl());
        if (legacyFaviconUrl == null) {
            // No URL -> Nothing to load.
            return null;
        }

        final LoadFaviconResult result = db.getFaviconForUrl(context, context.getContentResolver(), legacyFaviconUrl);
        if (result == null) {
            return null;
        }

        return result.getBestBitmap(request.getTargetSize());
    }
}
