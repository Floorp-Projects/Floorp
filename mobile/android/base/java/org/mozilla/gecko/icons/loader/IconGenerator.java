/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.TypedValue;

import org.mozilla.gecko.R;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.util.StringUtils;

/**
 * This loader will generate an icon in case no icon could be loaded. In order to do so this needs
 * to be the last loader that will be tried.
 */
public class IconGenerator implements IconLoader {
    // Mozilla's Visual Design Colour Palette
    // http://firefoxux.github.io/StyleGuide/#/visualDesign/colours
    private static final int[] COLORS = {
            0xFF9A4C00,
            0xFFAB008D,
            0xFF4C009C,
            0xFF002E9C,
            0xFF009EC2,
            0xFF009D02,
            0xFF51AB00,
            0xFF36385A,
    };

    @Override
    public IconResponse load(IconRequest request) {
        if (request.getIconCount() > 1) {
            // There are still other icons to try. We will only generate an icon if there's only one
            // icon left and all previous loaders have failed (assuming this is the last one).
            return null;
        }

        return generate(request.getContext(), request.getPageUrl(), request.getTargetSize(), request.getTextSize());
    }

    public static IconResponse generate(Context context, String pageURL) {
        return generate(context, pageURL, 0, 0);
    }

    /**
     * Generate default favicon for the given page URL.
     */
    public static IconResponse generate(final Context context, final String pageURL,
                                        int widthAndHeight, float textSize) {
        final Resources resources = context.getResources();
        if (widthAndHeight == 0) {
            widthAndHeight = resources.getDimensionPixelSize(R.dimen.favicon_bg);
        }
        final int roundedCorners = resources.getDimensionPixelOffset(R.dimen.favicon_corner_radius);

        final Bitmap favicon = Bitmap.createBitmap(widthAndHeight, widthAndHeight, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(favicon);

        final int color = pickColor(pageURL);

        final Paint paint = new Paint();
        paint.setColor(color);

        canvas.drawRoundRect(new RectF(0, 0, widthAndHeight, widthAndHeight), roundedCorners, roundedCorners, paint);

        paint.setColor(Color.WHITE);

        final String character = getRepresentativeCharacter(pageURL);

        if (textSize == 0) {
            // The text size is calculated dynamically based on the target icon size (1/8th). For an icon
            // size of 112dp we'd use a text size of 14dp (112 / 8).
            textSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                                                 widthAndHeight / 8,
                                                 resources.getDisplayMetrics());
        }

        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(textSize);
        paint.setAntiAlias(true);

        canvas.drawText(character,
                canvas.getWidth() / 2,
                (int) ((canvas.getHeight() / 2) - ((paint.descent() + paint.ascent()) / 2)),
                paint);

        return IconResponse.createGenerated(favicon, color);
    }

    /**
     * Get a representative character for the given URL.
     *
     * For example this method will return "f" for "http://m.facebook.com/foobar".
     */
    @VisibleForTesting static String getRepresentativeCharacter(String url) {
        if (TextUtils.isEmpty(url)) {
            return "?";
        }

        final String snippet = getRepresentativeSnippet(url);
        for (int i = 0; i < snippet.length(); i++) {
            char c = snippet.charAt(i);

            if (Character.isLetterOrDigit(c)) {
                return String.valueOf(Character.toUpperCase(c));
            }
        }

        // Nothing found..
        return "?";
    }

    /**
     * Return a color for this URL. Colors will be based on the host. URLs with the same host will
     * return the same color.
     */
    @VisibleForTesting static int pickColor(String url) {
        if (TextUtils.isEmpty(url)) {
            return COLORS[0];
        }

        final String snippet = getRepresentativeSnippet(url);
        final int color = Math.abs(snippet.hashCode() % COLORS.length);

        return COLORS[color];
    }

    /**
     * Get the representative part of the URL. Usually this is the host (without common prefixes).
     */
    private static String getRepresentativeSnippet(@NonNull String url) {
        Uri uri = Uri.parse(url);

        // Use the host if available
        String snippet = uri.getHost();

        if (TextUtils.isEmpty(snippet)) {
            // If the uri does not have a host (e.g. file:// uris) then use the path
            snippet = uri.getPath();
        }

        if (TextUtils.isEmpty(snippet)) {
            // If we still have no snippet then just return the question mark
            return "?";
        }

        // Strip common prefixes that we do not want to use to determine the representative characterS
        snippet = StringUtils.stripCommonSubdomains(snippet);

        return snippet;
    }
}
