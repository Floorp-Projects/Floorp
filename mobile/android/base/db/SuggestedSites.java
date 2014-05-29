/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MatrixCursor.RowBuilder;
import android.text.TextUtils;
import android.util.Log;

import java.io.IOException;
import java.lang.ref.SoftReference;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.RawResource;

/**
 * {@code SuggestedSites} provides API to get a list of locale-specific
 * suggested sites to be used in Fennec's top sites panel. It provides
 * only a single method to fetch the list as a {@code Cursor}. This cursor
 * will then be wrapped by {@code TopSitesCursorWrapper} to blend top,
 * pinned, and suggested sites in the UI. The returned {@code Cursor}
 * uses its own schema defined in {@code BrowserContract.SuggestedSites}
 * for clarity.
 *
 * Under the hood, {@code SuggestedSites} keeps reference to the
 * parsed list of sites to avoid reparsing the JSON file on every
 * {@code get()} call. This cached list is a soft reference and can
 * garbage collected at any moment.
 *
 * The default list of suggested sites is stored in a raw Android
 * resource ({@code R.raw.suggestedsites}) which is dynamically
 * generated at build time for each target locale.
 */
@RobocopTarget
public class SuggestedSites {
    private static final String LOGTAG = "GeckoSuggestedSites";

    private static final String[] COLUMNS = new String[] {
        BrowserContract.SuggestedSites._ID,
        BrowserContract.SuggestedSites.URL,
        BrowserContract.SuggestedSites.TITLE,
        BrowserContract.SuggestedSites.IMAGE_URL,
        BrowserContract.SuggestedSites.BG_COLOR
    };

    private static final String JSON_KEY_URL = "url";
    private static final String JSON_KEY_TITLE = "title";
    private static final String JSON_KEY_IMAGE_URL = "imageurl";
    private static final String JSON_KEY_BG_COLOR = "bgcolor";

    private static class Site {
        public final String url;
        public final String title;
        public final String imageUrl;
        public final String bgColor;

        public Site(String url, String title, String imageUrl, String bgColor) {
            this.url = url;
            this.title = title;
            this.imageUrl = imageUrl;
            this.bgColor = bgColor;
        }

        @Override
        public String toString() {
            return "{ url = " + url + "\n" +
                     "title = " + title + "\n" +
                     "imageUrl = " + imageUrl + "\n" +
                     "bgColor = " + bgColor + " }";
        }
    }

    private final Context context;
    private SoftReference<Map<String, Site>> cachedSites;

    public SuggestedSites(Context appContext) {
        context = appContext;
        cachedSites = new SoftReference<Map<String, Site>>(null);
    }

    private String loadFromFile() {
        // Do nothing for now
        return null;
    }

    private String loadFromResource() {
        try {
            return RawResource.getAsString(context, R.raw.suggestedsites);
        } catch (IOException e) {
            return null;
        }
    }

    /**
     * Refreshes the cached list of sites either from the default raw
     * source or standard file location. This will be called on every
     * cache miss during a {@code get()} call.
     */
    private Map<String, Site> refresh() {
        Log.d(LOGTAG, "Refreshing tiles from file");

        String jsonString = loadFromFile();
        if (TextUtils.isEmpty(jsonString)) {
            Log.d(LOGTAG, "No suggested sites file, loading from resource.");
            jsonString = loadFromResource();
        }

        Map<String, Site> sites = null;

        try {
            final JSONArray jsonSites = new JSONArray(jsonString);
            sites = new LinkedHashMap<String, Site>(jsonSites.length());

            final int count = jsonSites.length();
            for (int i = 0; i < count; i++) {
                final JSONObject jsonSite = (JSONObject) jsonSites.get(i);
                final String url = jsonSite.getString(JSON_KEY_URL);

                final Site site = new Site(url,
                                           jsonSite.getString(JSON_KEY_TITLE),
                                           jsonSite.getString(JSON_KEY_IMAGE_URL),
                                           jsonSite.getString(JSON_KEY_BG_COLOR));

                sites.put(url, site);
            }

            Log.d(LOGTAG, "Successfully parsed suggested sites.");
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to refresh suggested sites", e);
            return null;
        }

        // Update cached list of sites
        cachedSites = new SoftReference<Map<String, Site>>(Collections.unmodifiableMap(sites));

        // Return the refreshed list
        return sites;
    }

    private boolean isEnabled() {
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        return prefs.getBoolean(GeckoPreferences.PREFS_SUGGESTED_SITES, true);
    }

    private Site getSiteForUrl(String url) {
        Map<String, Site> sites = cachedSites.get();
        if (sites == null) {
            return null;
        }

        return sites.get(url);
    }

    /**
     * Returns a {@code Cursor} with the list of suggested websites.
     *
     * @param limit maximum number of suggested sites.
     */
    public Cursor get(int limit) {
        return get(limit, null);
    }

    /**
     * Returns a {@code Cursor} with the list of suggested websites.
     *
     * @param limit maximum number of suggested sites.
     * @param excludeUrls list of URLs to be excluded from the list.
     */
    public Cursor get(int limit, List<String> excludeUrls) {
        final MatrixCursor cursor = new MatrixCursor(COLUMNS);

        // Return an empty cursor if suggested sites have been
        // disabled by the user.
        if (!isEnabled()) {
            return cursor;
        }

        Map<String, Site> sites = cachedSites.get();
        if (sites == null) {
            Log.d(LOGTAG, "No cached sites, refreshing.");
            sites = refresh();
        }

        // Return empty cursor if there was an error when
        // loading the suggested sites or the list is empty.
        if (sites == null || sites.isEmpty()) {
            return cursor;
        }

        final int sitesCount = sites.size();
        Log.d(LOGTAG, "Number of suggested sites: " + sitesCount);

        final int maxCount = Math.min(limit, sitesCount);
        for (Site site : sites.values()) {
            if (cursor.getCount() == maxCount) {
                break;
            }

            if (excludeUrls != null && excludeUrls.contains(site.url)) {
                continue;
            }

            final RowBuilder row = cursor.newRow();
            row.add(-1);
            row.add(site.url);
            row.add(site.title);
            row.add(site.imageUrl);
            row.add(site.bgColor);
        }

        cursor.setNotificationUri(context.getContentResolver(),
                                  BrowserContract.SuggestedSites.CONTENT_URI);

        return cursor;
    }

    public boolean contains(String url) {
        return (getSiteForUrl(url) != null);
    }

    public String getImageUrlForUrl(String url) {
        final Site site = getSiteForUrl(url);
        return (site != null ? site.imageUrl : null);
    }

    public String getBackgroundColorForUrl(String url) {
        final Site site = getSiteForUrl(url);
        return (site != null ? site.bgColor : null);
    }
}