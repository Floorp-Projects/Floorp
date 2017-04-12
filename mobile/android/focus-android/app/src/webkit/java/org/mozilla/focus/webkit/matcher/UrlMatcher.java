/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit.matcher;


import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.util.JsonReader;

import org.mozilla.focus.R;
import org.mozilla.focus.webkit.matcher.util.FocusString;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class UrlMatcher implements  SharedPreferences.OnSharedPreferenceChangeListener {
    /**
     * Map of pref to blocking category (preference key -> Blocklist category name).
     */
    private final Map<String, String> categoryPrefMap;

    private static final String[] WEBFONT_EXTENSIONS = new String[]{
            ".woff2",
            ".woff",
            ".eot",
            ".ttf",
            ".otf"
    };

    private static final String WEBFONTS = "Webfonts";

    private static Map<String, String> loadDefaultPrefMap(final Context context) {
        Map<String, String> tempMap = new HashMap<>(5);

        tempMap.put(context.getString(R.string.pref_key_privacy_block_ads), "Advertising");
        tempMap.put(context.getString(R.string.pref_key_privacy_block_analytics), "Analytics");
        tempMap.put(context.getString(R.string.pref_key_privacy_block_social), "Social");
        tempMap.put(context.getString(R.string.pref_key_privacy_block_other), "Content");

        // This is a "fake" category - webfont handling is independent of the blocklists
        tempMap.put(context.getString(R.string.pref_key_performance_block_webfonts), WEBFONTS);

        return Collections.unmodifiableMap(tempMap);
    }

    private final Map<String, Trie> categories;
    private final Set<String> enabledCategories = new HashSet<>();

    private final EntityList entityList;
    // A cached list of previously matched URLs. This MUST be cleared whenever items are removed from enabledCategories.
    private final HashSet<String> previouslyMatched = new HashSet<>();
    // A cahced list of previously approved URLs. This MUST be cleared whenever items are added to enabledCategories.
    private final HashSet<String> previouslyUnmatched = new HashSet<>();

    private boolean blockWebfonts = true;

    public static UrlMatcher loadMatcher(final Context context, final int blockListFile, final int[] blockListOverrides, final int entityListFile) {
        final Map<String, String> categoryPrefMap = loadDefaultPrefMap(context);

        final Map<String, Trie> categoryMap = new HashMap<>(5);
        {
            InputStream inputStream = context.getResources().openRawResource(blockListFile);
            JsonReader jsonReader = new JsonReader(new InputStreamReader(inputStream));

            try {
                BlocklistProcessor.loadCategoryMap(jsonReader, categoryMap, BlocklistProcessor.ListType.BASE_LIST);

                jsonReader.close();
            } catch (IOException e) {
                throw new IllegalStateException("Unable to parse blacklist");
            }
        }

        if (blockListOverrides != null) {
            for (int i = 0; i < blockListOverrides.length; i++) {
                InputStream inputStream = context.getResources().openRawResource(blockListOverrides[i]);
                JsonReader jsonReader = new JsonReader(new InputStreamReader(inputStream));

                try {
                    BlocklistProcessor.loadCategoryMap(jsonReader, categoryMap, BlocklistProcessor.ListType.OVERRIDE_LIST);

                    jsonReader.close();
                } catch (IOException e) {
                    throw new IllegalStateException("Unable to parse override blacklist");
                }
            }
        }

        final EntityList entityList;
        {
            InputStream inputStream = context.getResources().openRawResource(entityListFile);
            JsonReader jsonReader = new JsonReader(new InputStreamReader(inputStream));

            try {
                entityList = EntityListProcessor.getEntityMapFromJSON(jsonReader);
            } catch (IOException e) {
                throw new IllegalStateException("Unable to parse entity list");
            } finally {
                try {
                    jsonReader.close();
                } catch (IOException e) {
                    // Nothing to do here...
                }
            }

        }

        return new UrlMatcher(context, categoryPrefMap, categoryMap, entityList);
    }

    /* package-private */ UrlMatcher(final Context context,
                                     @NonNull final Map<String, String> categoryPrefMap,
                                     @NonNull final Map<String, Trie> categoryMap,
                                     @Nullable final EntityList entityList) {
        this.categoryPrefMap = categoryPrefMap;
        this.entityList = entityList;
        this.categories = categoryMap;

        // Ensure all categories have been declared, and enable by default (loadPrefs() will then
        // enabled/disable categories that have actually been configured).
        for (final Map.Entry<String, Trie> entry: categoryMap.entrySet()) {
            if (!categoryPrefMap.values().contains(entry.getKey())) {
                throw new IllegalArgumentException("categoryMap contains undeclared category");
            }

            // Failsafe: enable all categories (we load preferences in the next step anyway)
            enabledCategories.add(entry.getKey());
        }

        loadPrefs(context);

        PreferenceManager.getDefaultSharedPreferences(context).registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(final SharedPreferences sharedPreferences, final String prefName) {
        final String categoryName = categoryPrefMap.get(prefName);

        if (categoryName != null) {
            final boolean prefValue = sharedPreferences.getBoolean(prefName, false);

            setCategoryEnabled(categoryName, prefValue);
        }
    }

    private void loadPrefs(final Context context) {
        final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);

        for (final Map.Entry<String, String> entry : categoryPrefMap.entrySet()) {
            final boolean prefValue = prefs.getBoolean(entry.getKey(), true);
            setCategoryEnabled(entry.getValue(), prefValue);
        }
    }

    @VisibleForTesting UrlMatcher(final String[] patterns) {
        final Map<String, String> map = new HashMap<>();
        map.put("default", "default");
        categoryPrefMap = Collections.unmodifiableMap(map);

        categories = new HashMap<>();

        buildMatcher(patterns);

        entityList = null;
    }

    /**
     * Only used for testing - uses a list of urls to populate a "default" category.
     * @param patterns
     */
    private void buildMatcher(String[] patterns) {
        final Trie defaultCategory;
        if (!categories.containsKey("default")) {
            defaultCategory = Trie.createRootNode();
            categories.put("default", defaultCategory);
        } else {
            defaultCategory = categories.get("default");
        }

        for (final String pattern : patterns) {
            defaultCategory.put(FocusString.create(pattern).reverse());
        }

        enabledCategories.add("default");
    }

    public Set<String> getCategories() {
        return categories.keySet();
    }

    public void setCategoryEnabled(final String category, final boolean enabled) {
        if (WEBFONTS.equals(category)) {
            blockWebfonts = enabled;
            return;
        }

        if (!getCategories().contains(category)) {
            throw new IllegalArgumentException("Can't enable/disable inexistant category");
        }

        if (enabled) {
            if (enabledCategories.contains(category)) {
                // Early return - nothing to do if the category is already enabled
                return;
            } else {
                enabledCategories.add(category);
                previouslyUnmatched.clear();
            }
        } else {
            if (!enabledCategories.contains(category)) {
                // Early return - nothing to do if the category is already disabled
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

        // We need to handle webfonts first: if they are blocked, then whitelists don't matter.
        // If they aren't blocked we still need to check domain blacklists below.
        if (blockWebfonts) {
            for (final String extension : WEBFONT_EXTENSIONS) {
                if (resourceURLString.endsWith(extension)) {
                    return true;
                }
            }
        }

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
