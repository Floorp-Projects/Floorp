/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons.loader;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.icons.decoders.FaviconDecoder;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.storage.FailureCache;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.ProxySelector;
import org.mozilla.gecko.util.StringUtils;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.HashSet;

/**
 * This loader implementation downloads icons from http(s) URLs.
 */
public class IconDownloader implements IconLoader {
    private static final String LOGTAG = "Gecko/Downloader";

    /**
     * The maximum number of http redirects (3xx) until we give up.
     */
    private static final int MAX_REDIRECTS_TO_FOLLOW = 5;

    /**
     * The default size of the buffer to use for downloading Favicons in the event no size is given
     * by the server. */
    private static final int DEFAULT_FAVICON_BUFFER_SIZE_BYTES = 25000;

    @Override
    public IconResponse load(IconRequest request) {
        if (request.shouldSkipNetwork()) {
            return null;
        }

        final String iconUrl = request.getBestIcon().getUrl();

        if (!StringUtils.isHttpOrHttps(iconUrl)) {
            return null;
        }

        try {
            final LoadFaviconResult result = downloadAndDecodeImage(request.getContext(), iconUrl);
            if (result == null) {
                return null;
            }

            final Bitmap bitmap = result.getBestBitmap(request.getTargetSize());
            if (bitmap == null) {
                return null;
            }

            return IconResponse.createFromNetwork(bitmap, iconUrl);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error reading favicon", e);
        } catch (OutOfMemoryError e) {
            Log.e(LOGTAG, "Insufficient memory to process favicon");
        }

        return null;
    }

    /**
     * Download the Favicon from the given URL and pass it to the decoder function.
     *
     * @param targetFaviconURL URL of the favicon to download.
     * @return A LoadFaviconResult containing the bitmap(s) extracted from the downloaded file, or
     *         null if no or corrupt data was received.
     */
    @VisibleForTesting
    @Nullable
    LoadFaviconResult downloadAndDecodeImage(Context context, String targetFaviconURL) {
        // Try the URL we were given.
        final HttpURLConnection connection = tryDownload(targetFaviconURL);
        if (connection == null) {
            return null;
        }

        InputStream stream = null;

        // Decode the image from the fetched response.
        try {
            stream = connection.getInputStream();
            return decodeImageFromResponse(context, stream, connection.getHeaderFieldInt("Content-Length", -1));
        } catch (IOException e) {
            Log.d(LOGTAG, "IOException while reading and decoding ixon", e);
            return null;
        } finally {
            // Close the stream and free related resources.
            IOUtils.safeStreamClose(stream);
            connection.disconnect();
        }
    }

    /**
     * Helper method for trying the download request to grab a Favicon.
     *
     * @param faviconURI URL of Favicon to try and download
     * @return The HttpResponse containing the downloaded Favicon if successful, null otherwise.
     */
    @Nullable
    private HttpURLConnection tryDownload(String faviconURI) {
        final HashSet<String> visitedLinkSet = new HashSet<>();
        visitedLinkSet.add(faviconURI);
        return tryDownloadRecurse(faviconURI, visitedLinkSet);
    }

    /**
     * Try to download from the favicon URL and recursively follow redirects.
     */
    @Nullable
    private HttpURLConnection tryDownloadRecurse(String faviconURI, HashSet<String> visited) {
        if (visited.size() == MAX_REDIRECTS_TO_FOLLOW) {
            return null;
        }

        HttpURLConnection connection = null;

        try {
            connection = connectTo(faviconURI);

            // Was the response a failure?
            final int status = connection.getResponseCode();

            // Handle HTTP status codes requesting a redirect.
            if (status >= 300 && status < 400) {
                final String newURI = connection.getHeaderField("Location");

                // Handle mad web servers.
                try {
                    if (newURI == null || newURI.equals(faviconURI)) {
                        return null;
                    }

                    if (visited.contains(newURI)) {
                        // Already been redirected here - abort.
                        return null;
                    }

                    visited.add(newURI);
                } finally {
                    connection.disconnect();
                }

                return tryDownloadRecurse(newURI, visited);
            }

            if (status >= 400) {
                // Client or Server error. Let's not retry loading from this URL again for some time.
                FailureCache.get().rememberFailure(faviconURI);

                connection.disconnect();
                return null;
            }
        } catch (IOException | URISyntaxException e) {
            if (connection != null) {
                connection.disconnect();
            }
            return null;
        }

        return connection;
    }

    @VisibleForTesting
    @NonNull
    HttpURLConnection connectTo(String uri) throws URISyntaxException, IOException {
        final HttpURLConnection connection = (HttpURLConnection) ProxySelector.openConnectionWithProxy(
                new URI(uri));

        connection.setRequestProperty("User-Agent", GeckoAppShell.getGeckoInterface().getDefaultUAString());

        // We implemented or own way of following redirects back when this code was using HttpClient.
        // Nowadays we should let HttpUrlConnection do the work - assuming that it doesn't follow
        // redirects in loops forever.
        connection.setInstanceFollowRedirects(false);

        return connection;
    }

    /**
     * Copies the favicon stream to a buffer and decodes downloaded content into bitmaps using the
     * FaviconDecoder.
     *
     * @param stream to decode
     * @param contentLength as reported by the server (or -1)
     * @return A LoadFaviconResult containing the bitmap(s) extracted from the downloaded file, or
     *         null if no or corrupt data were received.
     * @throws IOException If attempts to fully read the stream result in such an exception, such as
     *                     in the event of a transient connection failure.
     */
    @Nullable
    private LoadFaviconResult decodeImageFromResponse(Context context, InputStream stream, int contentLength) throws IOException {
        // This may not be provided, but if it is, it's useful.
        final int bufferSize;
        if (contentLength > 0) {
            // The size was reported and sane, so let's use that.
            // Integer overflow should not be a problem for Favicon sizes...
            bufferSize = contentLength + 1;
        } else {
            // No declared size, so guess and reallocate later if it turns out to be too small.
            bufferSize = DEFAULT_FAVICON_BUFFER_SIZE_BYTES;
        }

        // Read the InputStream into a byte[].
        final IOUtils.ConsumedInputStream result = IOUtils.readFully(stream, bufferSize);
        if (result == null) {
            return null;
        }

        // Having downloaded the image, decode it.
        return FaviconDecoder.decodeFavicon(context, result.getData(), 0, result.consumedLength);
    }
}
