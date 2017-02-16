/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit.matcher;

import android.util.ArrayMap;
import android.util.JsonReader;
import android.util.JsonToken;

import org.mozilla.focus.webkit.matcher.util.FocusString;

import java.io.IOException;
import java.util.Map;

public class BlocklistProcessor {

    public BlocklistProcessor(final JsonReader reader, final UrlMatcher matcher) throws IOException {
        reader.beginObject();

        while (reader.hasNext()) {
            JsonToken token = reader.peek();

            final String name = reader.nextName();

            if (name.equals("categories")) {
                extractCategories(reader, matcher);
            } else {
                reader.skipValue();
            }

        }

        reader.endObject();
    }

    private static void extractCategories(final JsonReader reader, final UrlMatcher matcher) throws IOException {
        reader.beginObject();

        // We ship 5 categories by default - so 5 is a good initial size
        final Map<String, Trie> categoryMap = new ArrayMap<>(5);

        while (reader.hasNext()) {
            final String categoryName = reader.nextName();

            final Trie categoryTrie = Trie.createRootNode();
            extractCategory(reader, categoryTrie);

            categoryMap.put(categoryName, categoryTrie);
        }

        matcher.putCategories(categoryMap);

        reader.endObject();
    }

    private static void extractCategory(final JsonReader reader, final Trie trie) throws IOException {
        reader.beginArray();

        while (reader.hasNext()) {
            extractSite(reader, trie);
        }

        reader.endArray();
    }

    private static void extractSite(final JsonReader reader, final Trie trie) throws IOException {
        reader.beginObject();

        final String siteName = reader.nextName();
        {
            reader.beginObject();

            while (reader.hasNext()) {
                final String siteURL = reader.nextName();
                JsonToken nextToken = reader.peek();

                if (nextToken.name().equals("STRING")) {
                    // Sometimes there's a "dnt" entry, with unspecified purpose.
                    reader.skipValue();
                } else {
                    reader.beginArray();

                    while (reader.hasNext()) {
                        final String blockURL = reader.nextString();
                        trie.put(FocusString.create(blockURL).reverse());
                    }

                    reader.endArray();
                }
            }

            reader.endObject();
        }

        reader.endObject();
    }
}
