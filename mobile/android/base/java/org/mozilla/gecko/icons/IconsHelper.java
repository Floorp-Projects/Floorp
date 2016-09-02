/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons;

import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.util.StringUtils;

import java.util.HashSet;

/**
 * Helper methods for icon related tasks.
 */
public class IconsHelper {
    private static final String LOGTAG = "Gecko/IconsHelper";

    // Mime types of things we are capable of decoding.
    private static final HashSet<String> sDecodableMimeTypes = new HashSet<>();

    // Mime types of things we are both capable of decoding and are container formats (May contain
    // multiple different sizes of image)
    private static final HashSet<String> sContainerMimeTypes = new HashSet<>();

    static {
        // MIME types extracted from http://filext.com - ostensibly all in-use mime types for the
        // corresponding formats.
        // ICO
        sContainerMimeTypes.add("image/vnd.microsoft.icon");
        sContainerMimeTypes.add("image/ico");
        sContainerMimeTypes.add("image/icon");
        sContainerMimeTypes.add("image/x-icon");
        sContainerMimeTypes.add("text/ico");
        sContainerMimeTypes.add("application/ico");

        // Add supported container types to the set of supported types.
        sDecodableMimeTypes.addAll(sContainerMimeTypes);

        // PNG
        sDecodableMimeTypes.add("image/png");
        sDecodableMimeTypes.add("application/png");
        sDecodableMimeTypes.add("application/x-png");

        // GIF
        sDecodableMimeTypes.add("image/gif");

        // JPEG
        sDecodableMimeTypes.add("image/jpeg");
        sDecodableMimeTypes.add("image/jpg");
        sDecodableMimeTypes.add("image/pipeg");
        sDecodableMimeTypes.add("image/vnd.swiftview-jpeg");
        sDecodableMimeTypes.add("application/jpg");
        sDecodableMimeTypes.add("application/x-jpg");

        // BMP
        sDecodableMimeTypes.add("application/bmp");
        sDecodableMimeTypes.add("application/x-bmp");
        sDecodableMimeTypes.add("application/x-win-bitmap");
        sDecodableMimeTypes.add("image/bmp");
        sDecodableMimeTypes.add("image/x-bmp");
        sDecodableMimeTypes.add("image/x-bitmap");
        sDecodableMimeTypes.add("image/x-xbitmap");
        sDecodableMimeTypes.add("image/x-win-bitmap");
        sDecodableMimeTypes.add("image/x-windows-bitmap");
        sDecodableMimeTypes.add("image/x-ms-bitmap");
        sDecodableMimeTypes.add("image/x-ms-bmp");
        sDecodableMimeTypes.add("image/ms-bmp");
    }

    /**
     * Helper method to getIcon the default Favicon URL for a given pageURL. Generally: somewhere.com/favicon.ico
     *
     * @param pageURL Page URL for which a default Favicon URL is requested
     * @return The default Favicon URL or null if no default URL could be guessed.
     */
    @Nullable
    public static String guessDefaultFaviconURL(String pageURL) {
        if (TextUtils.isEmpty(pageURL)) {
            return null;
        }

        // Special-casing for about: pages. The favicon for about:pages which don't provide a link tag
        // is bundled in the database, keyed only by page URL, hence the need to return the page URL
        // here. If the database ever migrates to stop being silly in this way, this can plausibly
        // be removed.
        if (AboutPages.isAboutPage(pageURL) || pageURL.startsWith("jar:")) {
            return pageURL;
        }

        if (!StringUtils.isHttpOrHttps(pageURL)) {
            // Guessing a default URL only makes sense for http(s) URLs.
            return null;
        }

        try {
            // Fall back to trying "someScheme:someDomain.someExtension/favicon.ico".
            Uri uri = Uri.parse(pageURL);
            if (uri.getAuthority().isEmpty()) {
                return null;
            }

            return uri.buildUpon()
                    .path("favicon.ico")
                    .clearQuery()
                    .fragment("")
                    .build()
                    .toString();
        } catch (Exception e) {
            Log.d(LOGTAG, "Exception getting default favicon URL");
            return null;
        }
    }

    /**
     * Helper function to determine if the provided mime type is that of a format that can contain
     * multiple image types. At time of writing, the only such type is ICO.
     * @param mimeType Mime type to check.
     * @return true if the given mime type is a container type, false otherwise.
     */
    public static boolean isContainerType(@NonNull String mimeType) {
        return sContainerMimeTypes.contains(mimeType);
    }

    /**
     * Helper function to determine if we can decode a particular mime type.
     *
     * @param imgType Mime type to check for decodability.
     * @return false if the given mime type is certainly not decodable, true if it might be.
     */
    public static boolean canDecodeType(@NonNull String imgType) {
        return sDecodableMimeTypes.contains(imgType);
    }
}
