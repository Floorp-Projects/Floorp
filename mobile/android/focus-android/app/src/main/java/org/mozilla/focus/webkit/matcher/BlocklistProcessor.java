/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit.matcher;

import android.util.JsonReader;
import android.util.JsonToken;

import java.io.IOException;

public class BlocklistProcessor {

    final UrlMatcher matcher;

    public BlocklistProcessor(final JsonReader reader, final UrlMatcher matcher) throws IOException {
        this.matcher = matcher;

        // TODO: refactor this class to match the EntityListProcessor example of a static method
        // returning a Trie of blacklisted URLs.
        reader.beginObject();

        while (reader.hasNext()) {
            JsonToken token = reader.peek();

            final String name = reader.nextName();

            if (name.equals("categories")) {
                extractCategories(reader);
            } else {
                reader.skipValue();
            }

        }

        reader.endObject();
    }

    private void extractCategories(final JsonReader reader) throws IOException {
        reader.beginObject();

        while (reader.hasNext()) {
            final String categoryName = reader.nextName();

            extractCategory(reader);
        }

        reader.endObject();
    }

    private void extractCategory(final JsonReader reader) throws IOException {
        reader.beginArray();

        while (reader.hasNext()) {
            extractSite(reader);
        }

        reader.endArray();
    }

    private void extractSite(final JsonReader reader) throws IOException {
        reader.beginObject();

        final String siteName = reader.nextName();
        {
            reader.beginObject();
            final String siteURL = reader.nextName();
            {
                reader.beginArray();

                while (reader.hasNext()) {
                    final String blockURL = reader.nextString();
                    matcher.putURL(blockURL);
                }

                reader.endArray();
            }

            reader.endObject();
        }

        reader.endObject();
    }
}
