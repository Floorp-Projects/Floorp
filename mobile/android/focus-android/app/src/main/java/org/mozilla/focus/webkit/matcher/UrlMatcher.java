/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit.matcher;


import android.content.Context;
import android.util.JsonReader;

import org.mozilla.focus.webkit.matcher.util.FocusString;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashSet;

public class UrlMatcher {

    private final Trie rootTrie = Trie.createRootNode();

    private final EntityList entityList;
    private final HashSet<String> previouslyMatched = new HashSet<>();
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
        for (final String pattern : patterns) {
            rootTrie.put(FocusString.create(pattern));
        }
    }

    /* package-private */ void putURL(final String url) {
        rootTrie.put(FocusString.create(url));
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
            // We must not cache entityList items (abd/or if we did, we'd have to clear the cache
            // on every single location change)
            return false;
        }

        if (previouslyMatched.contains(resourceURLString)) {
            return true;
        }

        for (int i = 0; i < resourceURLString.length() - 1; i++) {
            if (rootTrie.findNode(FocusString.create(resourceURLString.substring(i))) != null) {
                previouslyMatched.add(resourceURLString);
                return true;
            }
        }

        previouslyUnmatched.add(resourceURLString);
        return false;
    }
}
