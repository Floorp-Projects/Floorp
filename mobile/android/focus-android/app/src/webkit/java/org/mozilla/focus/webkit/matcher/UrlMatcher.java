/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit.matcher;


import android.content.Context;
import android.support.v4.util.ArrayMap;
import android.util.JsonReader;

import org.mozilla.focus.webkit.matcher.util.FocusString;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class UrlMatcher {

    private final Map<String, Trie> categories = new ArrayMap<>(5);
    private final Set<String> enabledCategories = new HashSet<>();

    private final EntityList entityList;
    // A cached list of previously matched URLs. This MUST be cleared whenever items are removed from enabledCategories.
    private final HashSet<String> previouslyMatched = new HashSet<>();
    // A cahced list of previously approved URLs. This MUST be cleared whenever items are added to enabledCategories.
    private final HashSet<String> previouslyUnmatched = new HashSet<>();

    public UrlMatcher(final Context context, final int blockListFile, final int entityListFile) {
        {
            InputStream inputStream = context.getResources().openRawResource(blockListFile);
            JsonReader jsonReader = new JsonReader(new InputStreamReader(inputStream));

            try {
                new BlocklistProcessor(jsonReader, this);

                jsonReader.close();
            } catch (IOException e) {
                throw new IllegalStateException("Unable to parse blacklist");
            }
        }

        {
            InputStream inputStream = context.getResources().openRawResource(entityListFile);
            JsonReader jsonReader = new JsonReader(new InputStreamReader(inputStream));

            try {
                entityList = EntityListProcessor.getEntityMapFromJSON(jsonReader);
            } catch (IOException e) {
                throw new IllegalStateException("Unable to parse entity list");
            }

        }
    }


    /* Test-only */
    /* package-private */ UrlMatcher(final String[] patterns) {
        buildMatcher(patterns);

        entityList = null;
    }

    private void buildMatcher(String[] patterns) {
        // TODO: metrics for load time?
        final Trie defaultCategory;
        if (!categories.containsKey("default")) {
            defaultCategory = Trie.createRootNode();
        } else {
            defaultCategory = categories.get("default");
        }

        for (final String pattern : patterns) {
            defaultCategory.put(FocusString.create(pattern).reverse());
        }
    }

    /* package-private */ void putCategories(final Map<String, Trie> categoryMap) {
        for (final Map.Entry<String, Trie> entry: categoryMap.entrySet()) {
            if (categories.containsKey(entry.getKey())) {
                throw new IllegalStateException("Can't add existing category");
            } else {
                categories.put(entry.getKey(), entry.getValue());
                // Failsafe: enable all categories
                enabledCategories.add(entry.getKey());
            }
        }
    }

    public Set<String> getCategories() {
        return categories.keySet();
    }

    public void setCategoryEnabled(final String category, final boolean enabled) {
        if (!getCategories().contains(category)) {
            throw new IllegalArgumentException("Can't enable/disable inexistant category");
        }

        if (enabled) {
            if (enabledCategories.contains(category)) {
                // Early return - nothing to do
                return;
            } else {
                enabledCategories.add(category);
                previouslyUnmatched.clear();
            }
        } else {
            if (!enabledCategories.contains(category)) {
                // Early return - nothing to do
                return;
            } else {
                enabledCategories.remove(category);
                previouslyMatched.clear();
            }

        }
    }

    public boolean matches(final String resourceURLString, final String pageURLString) {
        // TODO: metrics for lookup time?
        // TODO: always whitelist current domain - decide on how to handle subdomains / TLDs correctly first though?

        // Cached whitelisted items can be permitted now (but blacklisted needs to wait for the override / entity list)
        if (previouslyUnmatched.contains(resourceURLString)) {
            return false;
        }


        if (entityList != null &&
                entityList.isWhiteListed(pageURLString, resourceURLString)) {
            // We must not cache entityList items (and/or if we did, we'd have to clear the cache
            // on every single location change)
            return false;
        }

        if (previouslyMatched.contains(resourceURLString)) {
            return true;
        }

        try {
            final String host = new URL(resourceURLString).getHost().toString();
            final FocusString revhost = FocusString.create(host).reverse();

            for (final Map.Entry<String, Trie> category : categories.entrySet()) {
                if (enabledCategories.contains(category.getKey())) {
                    if (category.getValue().findNode(revhost) != null) {
                        previouslyMatched.add(resourceURLString);
                        return true;
                    }
                }
            }

        } catch (MalformedURLException e) {
            // In reality this should never happen - unless webkit were to pass us an invalid URL.
            // If we ever hit this in the wild, we might want to think our approach...
            throw new IllegalArgumentException("Unable to handle malformed resource URL");
        }

        previouslyUnmatched.add(resourceURLString);
        return false;
    }
}
