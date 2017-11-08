/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.res.AssetManager;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.support.annotation.VisibleForTesting;
import android.util.Base64;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

/**
 * A very simple parser for search plugins.
 */
/* package */ class SearchEngineParser {
    private static final String URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";
    private static final String URLTYPE_SEARCH_HTML  = "text/html";
    private static final String URL_REL_MOBILE = "mobile";
    private static final String SEARCH_TERM_VALUE = "{searchTerms}";

    private static final String IMAGE_URI_PREFIX = "data:image/png;base64,";

    public static SearchEngine load(AssetManager assetManager, String identifier, String path) throws IOException {
        try (final InputStream stream = assetManager.open(path)) {
            return load(identifier, stream);
        } catch (XmlPullParserException e) {
            throw new AssertionError("Parser exception while reading " + path, e);
        }
    }

    /* package */ @VisibleForTesting static SearchEngine load(String identifier, InputStream stream) throws IOException, XmlPullParserException {
        final SearchEngine searchEngine = new SearchEngine(identifier);

        XmlPullParser parser = XmlPullParserFactory.newInstance().newPullParser();
        parser.setInput(new InputStreamReader(stream, StandardCharsets.UTF_8));
        parser.next();

        readSearchPlugin(parser, searchEngine);

        return searchEngine;
    }

    private static void readSearchPlugin(XmlPullParser parser, SearchEngine searchEngine) throws XmlPullParserException, IOException {
        if (XmlPullParser.START_TAG != parser.getEventType()) {
            throw new XmlPullParserException("Expected start tag: " + parser.getPositionDescription());
        }

        final String name = parser.getName();
        if (!"SearchPlugin".equals(name) && !"OpenSearchDescription".equals(name)) {
            throw new XmlPullParserException("Expected <SearchPlugin> or <OpenSearchDescription> as root tag: "
                    + parser.getPositionDescription());
        }

        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }

            final String tag = parser.getName();

            if (tag.equals("ShortName")) {
                readShortName(parser, searchEngine);
            } else if (tag.equals("Url")) {
                readUrl(parser, searchEngine);
            } else if (tag.equals("Image")) {
                readImage(parser, searchEngine);
            } else {
                skip(parser);
            }
        }
    }

    private static void readUrl(XmlPullParser parser, SearchEngine searchEngine) throws XmlPullParserException, IOException {
        parser.require(XmlPullParser.START_TAG, null, "Url");

        final String type = parser.getAttributeValue(null, "type");
        final String template = parser.getAttributeValue(null, "template");
        final String rel = parser.getAttributeValue(null, "rel");

        Uri uri = Uri.parse(template);

        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }

            final String tag = parser.getName();

            if (tag.equals("Param")) {
                final String name = parser.getAttributeValue(null, "name");
                final String value = parser.getAttributeValue(null, "value");
                uri = uri.buildUpon().appendQueryParameter(name, value).build();
                parser.nextTag();
            } else {
                skip(parser);
            }
        }

        if (type.equals(URLTYPE_SEARCH_HTML)) {
            // Prefer mobile URIs.
            if (rel != null && rel.equals(URL_REL_MOBILE)) {
                searchEngine.resultsUris.add(0, uri);
            } else {
                searchEngine.resultsUris.add(uri);
            }
        } else if (type.equals(URLTYPE_SUGGEST_JSON)) {
            searchEngine.suggestUri = uri;
        }
    }

    private static void skip(XmlPullParser parser) throws XmlPullParserException, IOException {
        if (parser.getEventType() != XmlPullParser.START_TAG) {
            throw new IllegalStateException();
        }
        int depth = 1;
        while (depth != 0) {
            switch (parser.next()) {
                case XmlPullParser.END_TAG:
                    depth--;
                    break;
                case XmlPullParser.START_TAG:
                    depth++;
                    break;
                default:
                    // Do nothing - we're skipping content
            }
        }
    }

    private static void readShortName(XmlPullParser parser, SearchEngine searchEngine) throws IOException, XmlPullParserException {
        parser.require(XmlPullParser.START_TAG, null, "ShortName");
        if (parser.next() == XmlPullParser.TEXT) {
            searchEngine.name = parser.getText();
            parser.nextTag();
        }
    }

    private static void readImage(XmlPullParser parser, SearchEngine searchEngine) throws IOException, XmlPullParserException {
        parser.require(XmlPullParser.START_TAG, null, "Image");

        if (parser.next() != XmlPullParser.TEXT) {
            return;
        }

        final String uri = parser.getText();
        if (!uri.startsWith(IMAGE_URI_PREFIX)) {
            return;
        }

        final byte[] raw = Base64.decode(uri.substring(IMAGE_URI_PREFIX.length()), Base64.DEFAULT);

        searchEngine.icon = BitmapFactory.decodeByteArray(raw, 0, raw.length);

        parser.nextTag();
    }
}
