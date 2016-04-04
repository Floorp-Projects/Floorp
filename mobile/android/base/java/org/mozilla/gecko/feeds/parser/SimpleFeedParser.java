/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.parser;

import android.util.Log;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

import ch.boye.httpclientandroidlib.util.TextUtils;

/**
 * A super simple feed parser written for implementing "content notifications". This XML Pull Parser
 * can read ATOM and RSS feeds and returns an object describing the feed and the latest entry.
 */
public class SimpleFeedParser {
    /**
     * Generic exception that's thrown by the parser whenever a stream cannot be parsed.
     */
    public static class ParserException extends Exception {
        private static final long serialVersionUID = -6119538440219805603L;

        public ParserException(Throwable cause) {
            super(cause);
        }

        public ParserException(String message) {
            super(message);
        }
    }

    private static final String LOGTAG = "Gecko/FeedParser";

    private static final String TAG_RSS = "rss";
    private static final String TAG_FEED = "feed";
    private static final String TAG_RDF = "RDF";
    private static final String TAG_TITLE = "title";
    private static final String TAG_ITEM = "item";
    private static final String TAG_LINK = "link";
    private static final String TAG_ENTRY = "entry";
    private static final String TAG_PUBDATE = "pubDate";
    private static final String TAG_UPDATED = "updated";
    private static final String TAG_DATE = "date";
    private static final String TAG_SOURCE = "source";
    private static final String TAG_IMAGE = "image";
    private static final String TAG_CONTENT = "content";

    private class ParserState {
        public Feed feed;
        public Item currentItem;
        public boolean isRSS;
        public boolean isATOM;
        public boolean inSource;
        public boolean inImage;
        public boolean inContent;
    }

    public Feed parse(InputStream in) throws ParserException, IOException {
        final ParserState state = new ParserState();

        try {
            final XmlPullParserFactory factory = XmlPullParserFactory.newInstance();
            factory.setNamespaceAware(true);

            XmlPullParser parser = factory.newPullParser();
            parser.setInput(in, null);

            int eventType = parser.getEventType();

            while (eventType != XmlPullParser.END_DOCUMENT) {
                switch (eventType) {
                    case XmlPullParser.START_DOCUMENT:
                        handleStartDocument(state);
                        break;

                    case XmlPullParser.START_TAG:
                        handleStartTag(parser, state);
                        break;

                    case XmlPullParser.END_TAG:
                        handleEndTag(parser, state);
                        break;
                }

                eventType = parser.next();
            }
        } catch (XmlPullParserException e) {
            throw new ParserException(e);
        }

        if (!state.feed.isSufficientlyComplete()) {
            throw new ParserException("Feed is not sufficiently complete");
        }

        return state.feed;
    }

    private void handleStartDocument(ParserState state) {
        state.feed = new Feed();
    }

    private void handleStartTag(XmlPullParser parser, ParserState state) throws IOException, XmlPullParserException {
        switch (parser.getName()) {
            case TAG_RSS:
                state.isRSS = true;
                break;

            case TAG_FEED:
                state.isATOM = true;
                break;

            case TAG_RDF:
                // This is a RSS 1.0 feed
                state.isRSS = true;
                break;

            case TAG_ITEM:
            case TAG_ENTRY:
                state.currentItem = new Item();
                break;

            case TAG_TITLE:
                handleTitleStartTag(parser, state);
                break;

            case TAG_LINK:
                handleLinkStartTag(parser, state);
                break;

            case TAG_PUBDATE:
                handlePubDateStartTag(parser, state);
                break;

            case TAG_UPDATED:
                handleUpdatedStartTag(parser, state);
                break;

            case TAG_DATE:
                handleDateStartTag(parser, state);
                break;

            case TAG_SOURCE:
                state.inSource = true;
                break;

            case TAG_IMAGE:
                state.inImage = true;
                break;

            case TAG_CONTENT:
                state.inContent = true;
                break;
        }
    }

    private void handleEndTag(XmlPullParser parser, ParserState state) {
        switch (parser.getName()) {
            case TAG_ITEM:
            case TAG_ENTRY:
                handleItemOrEntryREndTag(state);
                break;

            case TAG_SOURCE:
                state.inSource = false;
                break;

            case TAG_IMAGE:
                state.inImage = false;
                break;

            case TAG_CONTENT:
                state.inContent = false;
                break;
        }
    }

    private void handleTitleStartTag(XmlPullParser parser, ParserState state) throws IOException, XmlPullParserException {
        if (state.inSource || state.inImage || state.inContent) {
            // We do not care about titles in <source>, <image> or <media> tags.
            return;
        }

        String title = getTextUntilEndTag(parser, TAG_TITLE);

        title = title.replaceAll("[\r\n]", " ");
        title = title.replaceAll("  +", " ");

        if (state.currentItem != null) {
            state.currentItem.setTitle(title);
        } else {
            state.feed.setTitle(title);
        }
    }

    private void handleLinkStartTag(XmlPullParser parser, ParserState state) throws IOException, XmlPullParserException {
        if (state.inSource || state.inImage) {
            // We do not care about links in <source> or <image> tags.
            return;
        }

        Map<String, String> attributes = fetchAttributes(parser);

        if (attributes.size() > 0) {
            String rel = attributes.get("rel");

            if (state.currentItem == null && "self".equals(rel)) {
                state.feed.setFeedURL(attributes.get("href"));
                return;
            }

            if (rel == null || "alternate".equals(rel)) {
                String type = attributes.get("type");
                if (type == null || type.equals("text/html")) {
                    String link = attributes.get("href");
                    if (TextUtils.isEmpty(link)) {
                        return;
                    }

                    if (state.currentItem != null) {
                        state.currentItem.setURL(link);
                    } else {
                        state.feed.setWebsiteURL(link);
                    }

                    return;
                }
            }
        }

        if (state.isRSS) {
            String link = getTextUntilEndTag(parser, TAG_LINK);
            if (TextUtils.isEmpty(link)) {
                return;
            }

            if (state.currentItem != null) {
                state.currentItem.setURL(link);
            } else {
                state.feed.setWebsiteURL(link);
            }
        }
    }

    private void handleItemOrEntryREndTag(ParserState state) {
        if (state.feed.getLastItem() == null || state.feed.getLastItem().getTimestamp() < state.currentItem.getTimestamp()) {
            // Only set this item as "last item" if we do not have an item yet or this item is newer.
            state.feed.setLastItem(state.currentItem);
        }

        state.currentItem = null;
    }

    private void handlePubDateStartTag(XmlPullParser parser, ParserState state) throws IOException, XmlPullParserException {
        if (state.currentItem == null) {
            return;
        }

        String pubDate = getTextUntilEndTag(parser, TAG_PUBDATE);
        if (TextUtils.isEmpty(pubDate)) {
            return;
        }

        // RFC-822
        SimpleDateFormat format = new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss Z", Locale.US);

        updateCurrentItemTimestamp(state, pubDate, format);
    }

    private void handleUpdatedStartTag(XmlPullParser parser, ParserState state) throws IOException, XmlPullParserException {
        if (state.inSource) {
            // We do not care about stuff in <source> tags.
            return;
        }

        if (state.currentItem == null) {
            // We are only interested in <updated> values of feed items.
            return;
        }

        String updated = getTextUntilEndTag(parser, TAG_UPDATED);
        if (TextUtils.isEmpty(updated)) {
            return;
        }

        SimpleDateFormat[] formats = new SimpleDateFormat[] {
            new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ", Locale.US),
            new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSSZ", Locale.US)
        };

        // Fix timezones SimpleDateFormat can't parse:
        // 2016-01-26T18:56:54Z -> 2016-01-26T18:56:54+0000 (Timezone: Z -> +0000)
        updated = updated.replaceFirst("Z$", "+0000");
        // 2016-01-26T18:56:54+01:00 -> 2016-01-26T18:56:54+0100 (Timezone: +01:00 -> +0100)
        updated = updated.replaceFirst("([0-9]{2})([\\+\\-])([0-9]{2}):([0-9]{2})$", "$1$2$3$4");

        updateCurrentItemTimestamp(state, updated, formats);
    }

    private void handleDateStartTag(XmlPullParser parser, ParserState state) throws IOException, XmlPullParserException {
        if (state.currentItem == null) {
            // We are only interested in <updated> values of feed items.
            return;
        }

        String text = getTextUntilEndTag(parser, TAG_DATE);
        if (TextUtils.isEmpty(text)) {
            return;
        }

        // Fix timezones SimpleDateFormat can't parse:
        // 2016-01-26T18:56:54+00:00 -> 2016-01-26T18:56:54+0000
        text = text.replaceFirst("([0-9]{2})([\\+\\-])([0-9]{2}):([0-9]{2})$", "$1$2$3$4");

        SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ", Locale.US);

        updateCurrentItemTimestamp(state, text, format);
    }

    private void updateCurrentItemTimestamp(ParserState state, String text, SimpleDateFormat... formats) {
        for (SimpleDateFormat format : formats) {
            try {
                Date date = format.parse(text);
                state.currentItem.setTimestamp(date.getTime());
                return;
            } catch (ParseException e) {
                Log.w(LOGTAG, "Could not parse 'updated': " + text);
            }
        }
    }

    private Map<String, String> fetchAttributes(XmlPullParser parser) {
        Map<String, String> attributes = new HashMap<>();

        for (int i = 0; i < parser.getAttributeCount(); i++) {
            attributes.put(parser.getAttributeName(i), parser.getAttributeValue(i));
        }

        return attributes;
    }

    private String getTextUntilEndTag(XmlPullParser parser, String tag) throws IOException, XmlPullParserException {
        StringBuilder builder = new StringBuilder();

        while (parser.next() != XmlPullParser.END_DOCUMENT) {
            if (parser.getEventType() == XmlPullParser.TEXT) {
                builder.append(parser.getText());
            } else if (parser.getEventType() == XmlPullParser.END_TAG && tag.equals(parser.getName())) {
                break;
            }
        }

        return builder.toString().trim();
    }
}
