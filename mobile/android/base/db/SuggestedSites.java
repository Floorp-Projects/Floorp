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
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import org.json.JSONArray;
import org.json.JSONObject;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.RawResource;
import org.mozilla.gecko.util.ThreadUtils;

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
 * {@code get()} call.
 *
 * The default list of suggested sites is stored in a raw Android
 * resource ({@code R.raw.suggestedsites}) which is dynamically
 * generated at build time for each target locale.
 *
 * Changes to the list of suggested sites are saved in SharedPreferences.
 */
@RobocopTarget
public class SuggestedSites {
    private static final String LOGTAG = "GeckoSuggestedSites";

    // SharedPreference key for suggested sites that should be hidden.
    public static final String PREF_SUGGESTED_SITES_HIDDEN = "suggestedSites.hidden";

    private static final String[] COLUMNS = new String[] {
        BrowserContract.SuggestedSites._ID,
        BrowserContract.SuggestedSites.URL,
        BrowserContract.SuggestedSites.TITLE
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
    private Map<String, Site> cachedSites;
    private Locale cachedLocale;
    private Set<String> cachedBlacklist;

    public SuggestedSites(Context appContext) {
        context = appContext;
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
    private void refresh() {
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
            return;
        }

        // Update cached list of sites
        cachedSites = Collections.unmodifiableMap(sites);
        cachedLocale = Locale.getDefault();
    }

    private boolean isEnabled() {
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        return prefs.getBoolean(GeckoPreferences.PREFS_SUGGESTED_SITES, true);
    }

    private synchronized Site getSiteForUrl(String url) {
        if (cachedSites == null) {
            return null;
        }

        return cachedSites.get(url);
    }

    /**
     * Returns a {@code Cursor} with the list of suggested websites.
     *
     * @param limit maximum number of suggested sites.
     */
    public Cursor get(int limit) {
        return get(limit, Locale.getDefault());
    }

    /**
     * Returns a {@code Cursor} with the list of suggested websites.
     *
     * @param limit maximum number of suggested sites.
     * @param locale the target locale.
     */
    public Cursor get(int limit, Locale locale) {
        return get(limit, locale, null);
    }

    /**
     * Returns a {@code Cursor} with the list of suggested websites.
     *
     * @param limit maximum number of suggested sites.
     * @param excludeUrls list of URLs to be excluded from the list.
     */
    public Cursor get(int limit, List<String> excludeUrls) {
        return get(limit, Locale.getDefault(), excludeUrls);
    }

    /**
     * Returns a {@code Cursor} with the list of suggested websites.
     *
     * @param limit maximum number of suggested sites.
     * @param locale the target locale.
     * @param excludeUrls list of URLs to be excluded from the list.
     */
    public synchronized Cursor get(int limit, Locale locale, List<String> excludeUrls) {
        final MatrixCursor cursor = new MatrixCursor(COLUMNS);

        // Return an empty cursor if suggested sites have been
        // disabled by the user.
        if (!isEnabled()) {
            return cursor;
        }

        if (cachedSites == null || !locale.equals(cachedLocale)) {
            Log.d(LOGTAG, "No cached sites, refreshing.");
            refresh();
        }

        // Return empty cursor if there was an error when
        // loading the suggested sites or the list is empty.
        if (cachedSites == null || cachedSites.isEmpty()) {
            return cursor;
        }

        excludeUrls = includeBlacklist(excludeUrls);

        final int sitesCount = cachedSites.size();
        Log.d(LOGTAG, "Number of suggested sites: " + sitesCount);

        final int maxCount = Math.min(limit, sitesCount);
        for (Site site : cachedSites.values()) {
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

    private Set<String> loadBlacklist() {
        Log.d(LOGTAG, "Loading blacklisted suggested sites from SharedPreferences.");
        final Set<String> blacklist = new HashSet<String>();

        final SharedPreferences preferences = GeckoSharedPrefs.forProfile(context);
        final String sitesString = preferences.getString(PREF_SUGGESTED_SITES_HIDDEN, null);

        if (sitesString != null) {
            for (String site : sitesString.trim().split(" ")) {
                blacklist.add(Uri.decode(site));
            }
        }

        return blacklist;
    }

    private List<String> includeBlacklist(List<String> originalList) {
        if (cachedBlacklist == null) {
            cachedBlacklist = loadBlacklist();
        }

        if (cachedBlacklist.isEmpty()) {
            return originalList;
        }

        if (originalList == null) {
            originalList = new ArrayList<String>();
        }

        originalList.addAll(cachedBlacklist);
        return originalList;
    }

    /**
     * Blacklist a suggested site so it will no longer be returned as a suggested site.
     * This method should only be called from a background thread because it may write
     * to SharedPreferences.
     *
     * Urls that are not Suggested Sites are ignored.
     *
     * @param url String url of site to blacklist
     * @return true is blacklisted, false otherwise
     */
    public synchronized boolean hideSite(String url) {
        ThreadUtils.assertNotOnUiThread();

        if (cachedSites == null) {
            refresh();
            if (cachedSites == null) {
                Log.w(LOGTAG, "Could not load suggested sites!");
                return false;
            }
        }

        if (cachedSites.containsKey(url)) {
            if (cachedBlacklist == null) {
                cachedBlacklist = loadBlacklist();
            }

            // Check if site has already been blacklisted, just in case.
            if (!cachedBlacklist.contains(url)) {

                saveToBlacklist(url);
                cachedBlacklist.add(url);

                return true;
            }
        }

        return false;
    }

    private void saveToBlacklist(String url) {
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        final String prefString = prefs.getString(PREF_SUGGESTED_SITES_HIDDEN, "");
        final String siteString = prefString.concat(" " + Uri.encode(url));
        prefs.edit().putString(PREF_SUGGESTED_SITES_HIDDEN, siteString).commit();
    }
}
