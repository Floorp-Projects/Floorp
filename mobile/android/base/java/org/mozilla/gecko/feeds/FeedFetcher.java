/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.gecko.feeds.parser.Feed;
import org.mozilla.gecko.feeds.parser.SimpleFeedParser;
import org.mozilla.gecko.util.IOUtils;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

import ch.boye.httpclientandroidlib.util.TextUtils;

/**
 * Helper class for fetching and parsing a feed.
 */
public class FeedFetcher {
    private static final int CONNECT_TIMEOUT = 15000;
    private static final int READ_TIMEOUT = 15000;

    public static class FeedResponse {
        public final Feed feed;
        public final String etag;
        public final String lastModified;

        public FeedResponse(Feed feed, String etag, String lastModified) {
            this.feed = feed;
            this.etag = etag;
            this.lastModified = lastModified;
        }
    }

    /**
     * Fetch and parse a feed from the given URL. Will return null if fetching or parsing failed.
     */
    public static FeedResponse fetchAndParseFeed(String url) {
        return fetchAndParseFeedIfModified(url, null, null);
    }

    /**
     * Fetch and parse a feed from the given URL using the given ETag and "Last modified" value.
     *
     * Will return null if fetching or parsing failed. Will also return null if the feed has not
     * changed (ETag / Last-Modified-Since).
     *
     * @param eTag The ETag from the last fetch or null if no ETag is available (will always fetch feed)
     * @param lastModified The "Last modified" header from the last time the feed has been fetch or
     *                     null if no value is available (will always fetch feed)
     * @return A FeedResponse or null if no feed could be fetched (error or no new version available)
     */
    @Nullable
    public static FeedResponse fetchAndParseFeedIfModified(@NonNull String url, @Nullable String eTag, @Nullable String lastModified) {
        HttpURLConnection connection = null;
        InputStream stream = null;

        try {
            connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setInstanceFollowRedirects(true);
            connection.setConnectTimeout(CONNECT_TIMEOUT);
            connection.setReadTimeout(READ_TIMEOUT);

            if (!TextUtils.isEmpty(eTag)) {
                connection.setRequestProperty("If-None-Match", eTag);
            }

            if (!TextUtils.isEmpty(lastModified)) {
                connection.setRequestProperty("If-Modified-Since", lastModified);
            }

            final int statusCode = connection.getResponseCode();

            if (statusCode != HttpURLConnection.HTTP_OK) {
                return null;
            }

            String responseEtag = connection.getHeaderField("ETag");
            if (!TextUtils.isEmpty(responseEtag) && responseEtag.startsWith("W/")) {
                // Weak ETag, get actual ETag value
                responseEtag = responseEtag.substring(2);
            }

            final String updatedLastModified = connection.getHeaderField("Last-Modified");

            stream = new BufferedInputStream(connection.getInputStream());

            final SimpleFeedParser parser = new SimpleFeedParser();
            final Feed feed = parser.parse(stream);

            return new FeedResponse(feed, responseEtag, updatedLastModified);
        } catch (IOException e) {
            return null;
        } catch (SimpleFeedParser.ParserException e) {
            return null;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            IOUtils.safeStreamClose(stream);
        }
    }
}
