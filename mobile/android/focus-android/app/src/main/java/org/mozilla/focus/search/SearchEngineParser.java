/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Base64;

import org.mozilla.focus.utils.IOUtils;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

/**
 * A very simple parser for search plugins.
 */
public class SearchEngineParser {
    private static final String TAG_SHORT_NAME = "ShortName";
    private static final String TAG_IMAGE = "Image";

    private static final String IMAGE_URI_PREFIX = "data:image/png;base64,";

    public static SearchEngine load(AssetManager assetManager, String path) throws IOException {
        final SearchEngine searchEngine = new SearchEngine();

        final InputStream stream = assetManager.open(path);

        try {
            XmlPullParser parser = XmlPullParserFactory.newInstance().newPullParser();
            parser.setInput(new InputStreamReader(stream));

            int eventType = parser.getEventType();

            while (eventType != XmlPullParser.END_DOCUMENT) {
                if (eventType == XmlPullParser.START_TAG && TAG_SHORT_NAME.equals(parser.getName())) {
                    searchEngine.name = consumeText(parser);
                }

                if (eventType == XmlPullParser.START_TAG && TAG_IMAGE.equals(parser.getName())) {
                    searchEngine.icon = decodeBitmap(parser);
                }

                eventType = parser.next();
            }
        } catch (XmlPullParserException e) {
            throw new AssertionError("Parser exception while reading " + path, e);
        } finally {
            IOUtils.safeClose(stream);
        }

        return searchEngine;
    }

    private static String consumeText(XmlPullParser parser) throws IOException, XmlPullParserException {
        nextUntilEvent(parser, XmlPullParser.TEXT);
        return parser.getText();
    }

    private static Bitmap decodeBitmap(XmlPullParser parser) throws IOException, XmlPullParserException {
        nextUntilEvent(parser, XmlPullParser.TEXT);

        final String uri = parser.getText();
        if (!uri.startsWith(IMAGE_URI_PREFIX)) {
            return null;
        }

        final byte[] raw = Base64.decode(uri.substring(IMAGE_URI_PREFIX.length()), Base64.DEFAULT);

        return BitmapFactory.decodeByteArray(raw, 0, raw.length);
    }

    private static void nextUntilEvent(XmlPullParser parser, int expectedEventType) throws IOException, XmlPullParserException {
        int eventType = parser.getEventType();

        while (eventType != expectedEventType && eventType != XmlPullParser.END_DOCUMENT) {
            eventType = parser.next();
        }
    }
}
