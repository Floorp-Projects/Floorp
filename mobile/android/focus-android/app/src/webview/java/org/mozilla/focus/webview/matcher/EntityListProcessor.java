/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webview.matcher;


import android.util.JsonReader;

import org.mozilla.focus.webview.matcher.util.FocusString;

import java.io.IOException;
import java.util.ArrayList;

/**
 * Parses an entitylist json file, and returns an EntityList representation thereof.
 */
/* package-private */ class EntityListProcessor {

    private final EntityList entityMap = new EntityList();

    public static EntityList getEntityMapFromJSON(final JsonReader reader) throws IOException {
        EntityListProcessor processor = new EntityListProcessor(reader);

        return processor.entityMap;
    }

    private EntityListProcessor(final JsonReader reader) throws IOException {
        reader.beginObject();

        while (reader.hasNext()) {
            // We can get the siteName using reader.nextName() here
            reader.skipValue();

            handleSite(reader);
        }

        reader.endObject();
    }

    private void handleSite(final JsonReader reader) throws IOException {
        reader.beginObject();

        final Trie whitelist = Trie.createRootNode();
        final ArrayList<String> propertyList = new ArrayList<>();

        while (reader.hasNext()) {
            final String itemName = reader.nextName();

            if (itemName.equals("properties")) {
                reader.beginArray();

                while (reader.hasNext()) {
                    propertyList.add(reader.nextString());
                }

                reader.endArray();
            } else if (itemName.equals("resources")) {
                reader.beginArray();

                while (reader.hasNext()) {
                    final FocusString revhost = FocusString.create(reader.nextString()).reverse();

                    whitelist.put(revhost);
                }

                reader.endArray();
            }
        }

        for (final String property : propertyList) {
            final FocusString revhost = FocusString.create(property).reverse();

            entityMap.putWhiteList(revhost, whitelist);
        }

        reader.endObject();
    }
}
