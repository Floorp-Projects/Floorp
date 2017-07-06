/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.TypedValue;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.UrlUtils;

/* package */ class IconGenerator {
    private static final int INNER_ICON_SIZE = 96;
    private static final int TEXT_SIZE_DP = 16;
    private static final int MINIMUM_INNER_ICON_SIZE = 32;
    private static final int GENERATED_ICON_ROUNDED_CORNERS = 5;

    // http://design.firefox.com/photon/visual/color.html#blue
    // Shade 60 colors
    private static final int[] COLORS = {
            0xFF0060df, // Blue 60
            0xFF00c8d7, // Teal 60
            0xFFed00b5, // Magenta 60
            0xFF12bc00, // Green 60
            0xFFd7b600, // Yellow 60
            0xFFd70022, // Red 60
            0xFF8000d7, // Purple 60
    };

    /**
     * Use the given raw website icon and generate a launcher icon from it. If the given icon is null
     * or too small then an icon will be generated based on the website's URL. The icon will be drawn
     * on top of a generic launcher icon shape that we provide.
     */
    /* package */ static Bitmap generateLauncherIcon(Context context, Bitmap rawIcon, String url) {
        final BitmapFactory.Options options = new BitmapFactory.Options();
        options.inMutable = true;

        final Bitmap shape = BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_homescreen_shape, options);

        final Bitmap innerIcon;

        if (rawIcon == null || rawIcon.getWidth() < MINIMUM_INNER_ICON_SIZE || rawIcon.getHeight() < MINIMUM_INNER_ICON_SIZE) {
            innerIcon = generateInnerIcon(context, url);
        } else {
            final int targetWidth = Math.min(rawIcon.getWidth() * 2, INNER_ICON_SIZE);
            final int targetHeight = Math.min(rawIcon.getHeight() * 2, INNER_ICON_SIZE);

            innerIcon = Bitmap.createScaledBitmap(rawIcon, targetWidth, targetHeight, true);
        }

        int drawX = (shape.getWidth() / 2) - (innerIcon.getWidth() / 2);
        int drawY = (shape.getHeight() / 2) - (innerIcon.getHeight() / 2);

        final Canvas canvas = new Canvas(shape);
        canvas.drawBitmap(innerIcon, drawX, drawY, new Paint());

        return shape;
    }

    /**
     * Generate an icon for this website based on the URL.
     */
    private static Bitmap generateInnerIcon(Context context, String url) {
        final Bitmap bitmap = Bitmap.createBitmap(INNER_ICON_SIZE, INNER_ICON_SIZE, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);

        final Paint paint = new Paint();
        paint.setColor(pickColor(url));

        canvas.drawRoundRect(new RectF(0, 0, INNER_ICON_SIZE, INNER_ICON_SIZE),
                GENERATED_ICON_ROUNDED_CORNERS, GENERATED_ICON_ROUNDED_CORNERS, paint);

        final String character = getRepresentativeCharacter(url);

        final float textSize = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, TEXT_SIZE_DP, context.getResources().getDisplayMetrics());

        paint.setColor(Color.WHITE);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(textSize);
        paint.setAntiAlias(true);

        canvas.drawText(character,
                canvas.getWidth() / 2,
                (int) ((canvas.getHeight() / 2) - ((paint.descent() + paint.ascent()) / 2)),
                paint);

        return bitmap;
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
        snippet = UrlUtils.stripCommonSubdomains(snippet);

        return snippet;
    }

    /**
     * Return a color for this URL. Colors will be based on the host. URLs with the same host will
     * return the same color.
     */
    @VisibleForTesting
    static int pickColor(String url) {
        if (TextUtils.isEmpty(url)) {
            return COLORS[0];
        }

        final String snippet = getRepresentativeSnippet(url);
        final int color = Math.abs(snippet.hashCode() % COLORS.length);

        return COLORS[color];
    }
}
