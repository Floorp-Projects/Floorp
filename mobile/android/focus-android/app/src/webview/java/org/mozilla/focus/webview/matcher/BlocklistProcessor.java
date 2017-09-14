/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webview.matcher;

import android.util.JsonReader;
import android.util.JsonToken;

import org.mozilla.focus.webview.matcher.util.FocusString;

import java.io.IOException;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class BlocklistProcessor {

    final static String SOCIAL = "Social";
    final static String DISCONNECT = "Disconnect";

    private static final Set<String> IGNORED_CATEGORIES;

    static {
        final Set<String> ignored = new HashSet<>();

        ignored.add("Legacy Disconnect");
        ignored.add("Legacy Content");

        IGNORED_CATEGORIES = Collections.unmodifiableSet(ignored);
    }

    /**
     * The sites in the "Disconnect" list that should be moved into "Social"
     */
    private static final Set<String> DISCONNECT_MOVED;

    static {
        final Set<String> moved = new HashSet<>();

        moved.add("Facebook");
        moved.add("Twitter");

        DISCONNECT_MOVED = Collections.unmodifiableSet(moved);
    }

    public enum ListType {
        BASE_LIST,
        OVERRIDE_LIST
    }

    public static Map<String, Trie> loadCategoryMap(final JsonReader reader, final Map<String, Trie> categoryMap, final ListType listType) throws IOException {
        reader.beginObject();

        while (reader.hasNext()) {
            final String name = reader.nextName();

            if (name.equals("categories")) {
                extractCategories(reader, categoryMap, listType);
            } else {
                reader.skipValue();
            }
        }

        reader.endObject();

        return categoryMap;
    }

    private interface UrlListCallback {
        void put(final String url, final String siteOwner);
    }

    private static class ListCallback implements UrlListCallback {
        final List<String> list;
        final Set<String> desiredOwners;

        /**
         * @param desiredOwners A set containing all the site owners that should be stored in the list.
         * Corresponds to the group owners listed in blocklist.json (e.g. "Facebook", "Twitter", etc.)
         */
        ListCallback(final List<String> list, final Set<String> desiredOwners) {
            this.list = list;
            this.desiredOwners = desiredOwners;
        }

        @Override
        public void put(final String url, final String siteOwner) {
            if (desiredOwners.contains(siteOwner)) {
                list.add(url);
            }
        }
    }

    private static class TrieCallback implements UrlListCallback {
        final Trie trie;

        TrieCallback(final Trie trie) {
            this.trie = trie;
        }

        @Override
        public void put(final String url, final String siteOwner) {
            trie.put(FocusString.create(url).reverse());
        }
    }

    private static void extractCategories(final JsonReader reader, final Map<String, Trie> categoryMap, final ListType listType) throws IOException {
        reader.beginObject();

        final List<String> socialOverrides = new LinkedList<String>();

        while (reader.hasNext()) {
            final String categoryName = reader.nextName();

            if (IGNORED_CATEGORIES.contains(categoryName)) {
                reader.skipValue();
            } else if (categoryName.equals(DISCONNECT)) {
                // We move these items into a different list, see below
                ListCallback callback = new ListCallback(socialOverrides, DISCONNECT_MOVED);
                extractCategory(reader, callback);
            } else {
                final Trie categoryTrie;

                if (listType == ListType.BASE_LIST) {
                    if (categoryMap.containsKey(categoryName)) {
                        throw new IllegalStateException("Cannot insert already loaded category");
                    }

                    categoryTrie = Trie.createRootNode();
                    categoryMap.put(categoryName, categoryTrie);
                } else {
                    categoryTrie = categoryMap.get(categoryName);

                    if (categoryTrie == null) {
                        throw new IllegalStateException("Cannot add override items to nonexistent category");
                    }
                }

                final TrieCallback callback = new TrieCallback(categoryTrie);

                extractCategory(reader, callback);
            }
        }

        final Trie socialTrie = categoryMap.get(SOCIAL);
        if (socialTrie == null && listType == ListType.BASE_LIST) {
            throw new IllegalStateException("Expected social list to exist. Can't copy FB/Twitter into non-existing list");
        }

        for (final String url : socialOverrides) {
            socialTrie.put(FocusString.create(url).reverse());
        }

        reader.endObject();
    }

    private static void extractCategory(final JsonReader reader, final UrlListCallback callback) throws IOException {
        reader.beginArray();

        while (reader.hasNext()) {
            extractSite(reader, callback);
        }

        reader.endArray();
    }

    private static void extractSite(final JsonReader reader, final UrlListCallback callback) throws IOException {
        reader.beginObject();

        final String siteOwner = reader.nextName();
        {
            reader.beginObject();

            while (reader.hasNext()) {
                // We can get the site name using reader.nextName() here:
                reader.skipValue();

                JsonToken nextToken = reader.peek();

                if (nextToken.name().equals("STRING")) {
                    // Sometimes there's a "dnt" entry, with unspecified purpose.
                    reader.skipValue();
                } else {
                    reader.beginArray();

                    while (reader.hasNext()) {
                        final String blockURL = reader.nextString();
                        callback.put(blockURL, siteOwner);
                    }

                    reader.endArray();
                }
            }

            reader.endObject();
        }

        reader.endObject();
    }
}
