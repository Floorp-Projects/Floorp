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
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.TypedValue;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.UrlUtils;

public class IconGenerator {
    private static final int ICON_SIZE = 96;
    private static final int TEXT_SIZE_DP = 36;

    /**
     * Use the given raw website icon and generate a launcher icon from it. If the given icon is null
     * or too small then an icon will be generated based on the website's URL. The icon will be drawn
     * on top of a generic launcher icon shape that we provide.
     */
    public static Bitmap generateLauncherIcon(Context context, String url) {
        final BitmapFactory.Options options = new BitmapFactory.Options();
        options.inMutable = true;

        final Bitmap shape = BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_homescreen_shape, options);

        final Bitmap icon = generateIcon(context, url);

        int drawX = (shape.getWidth() / 2) - (icon.getWidth() / 2);
        int drawY = (shape.getHeight() / 2) - (icon.getHeight() / 2);

        final Canvas canvas = new Canvas(shape);
        canvas.drawBitmap(icon, drawX, drawY, new Paint());

        return shape;
    }

    /**
     * Generate an icon for this website based on the URL.
     */
    private static Bitmap generateIcon(Context context, String url) {
        final Bitmap bitmap = Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);

        final Paint paint = new Paint();

        final String character = getRepresentativeCharacter(url);

        final float textSize = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, TEXT_SIZE_DP, context.getResources().getDisplayMetrics());

        paint.setColor(Color.WHITE);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(textSize);
        paint.setAntiAlias(true);

        canvas.drawText(character,
                canvas.getWidth() / 2.0f,
                ((canvas.getHeight() / 2.0f) - ((paint.descent() + paint.ascent()) / 2.0f)),
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
}
