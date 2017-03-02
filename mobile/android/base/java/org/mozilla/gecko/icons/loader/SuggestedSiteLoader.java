/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.icons.loader;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.home.ImageLoader;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;

import java.io.IOException;

public class SuggestedSiteLoader implements IconLoader {
    public static final String SUGGESTED_SITE_TOUCHTILE = "suggestedsitetile:";

    private static final String LOGTAG = SuggestedSiteLoader.class.getSimpleName();

    @Nullable
    @Override
    public IconResponse load(IconRequest request) {
        if (request.shouldSkipDisk()) {
            return null;
        }

        final String iconUrl = request.getBestIcon().getUrl();

        if (iconUrl.startsWith(SUGGESTED_SITE_TOUCHTILE)) {
            return buildIcon(request.getContext(), iconUrl.substring(SUGGESTED_SITE_TOUCHTILE.length()), request.getTargetSize());
        }

        return null;
    }

    @VisibleForTesting
    Bitmap loadBitmap(final Context context, final String iconURL) throws IOException {
        return ImageLoader.with(context)
                .load(iconURL)
                .noFade()
                .get();
    }

    private IconResponse buildIcon(final Context context, final String siteURL, final int targetSize) {
        final SuggestedSites suggestedSites = BrowserDB.from(context).getSuggestedSites();

        if (suggestedSites == null) {
            // See longer explanation in SuggestedSitePreparer: suggested sites aren't always loaded.
            // If they aren't, SuggestedSitePreparer won't suggest any bundled icons so we should
            // never try to load them anyway, but we should double check here to be completely safe.
            return null;
        }

        final String iconLocation = suggestedSites.getImageUrlForUrl(siteURL);
        final String backgroundColorString = suggestedSites.getBackgroundColorForUrl(siteURL);

        if (iconLocation == null || backgroundColorString == null) {
            // There's little we can do if loading a bundled resource fails: this failure could
            // be caused by a distribution (as opposed to Gecko), so we should just shout loudly,
            // as opposed to crashing:
            Log.e(LOGTAG, "Unable to find tile data definitions for site:" + siteURL);
            return null;
        }

        final int backgroundColor = Color.parseColor(backgroundColorString);

        try {
            final Bitmap foreground = loadBitmap(context, iconLocation);

            // Our supplied tile icons are bigger than the max favicon size. They are also not square,,
            // so we scale them down and fit them into a square (i.e. centering in their smaller dimension):
            final Bitmap output = Bitmap.createBitmap(targetSize, targetSize, foreground.getConfig());
            final Canvas canvas = new Canvas(output);
            canvas.drawColor(backgroundColor);

            final Rect src = new Rect(0, 0, foreground.getWidth(), foreground.getHeight());

            // And we draw the icon in the middle of that square:
            final float scaleFactor = targetSize * 1.0f / Math.max(foreground.getHeight(), foreground.getWidth());

            final int heightDelta = targetSize - (int) (scaleFactor * foreground.getHeight());
            final int widthDelta = targetSize - (int) (scaleFactor * foreground.getWidth());

            final Rect dst = new Rect(widthDelta / 2, heightDelta / 2, output.getWidth() - (widthDelta / 2), output.getHeight() - (heightDelta / 2));

            // Interpolate when painting:
            final Paint paint = new Paint();
            paint.setFilterBitmap(true);
            paint.setAntiAlias(true);
            paint.setDither(true);

            canvas.drawBitmap(foreground, src, dst, paint);

            final IconResponse response = IconResponse.create(output);
            response.updateColor(backgroundColor);

            return response;
        } catch (IOException e) {
            // Same as above: if we can't load the data, shout - but don't crash:
            Log.e(LOGTAG, "Unable to load tile data for site:" + siteURL + " at location:" + iconLocation);
        }

        return null;
    }
}
