/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.Context;
import android.content.ContentResolver;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MatrixCursor.RowBuilder;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Scanner;
import java.util.Set;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.RawResource;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.preferences.GeckoPreferences;

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
    public static final String PREF_SUGGESTED_SITES_HIDDEN = GeckoPreferences.NON_PREF_PREFIX + "suggestedSites.hidden";
    public static final String PREF_SUGGESTED_SITES_HIDDEN_OLD = "suggestedSites.hidden";

    // Locale used to generate the current suggested sites.
    public static final String PREF_SUGGESTED_SITES_LOCALE = GeckoPreferences.NON_PREF_PREFIX + "suggestedSites.locale";
    public static final String PREF_SUGGESTED_SITES_LOCALE_OLD = "suggestedSites.locale";

    // File in profile dir with the list of suggested sites.
    private static final String FILENAME = "suggestedsites.json";

    private static final String[] COLUMNS = new String[] {
        BrowserContract.SuggestedSites._ID,
        BrowserContract.SuggestedSites.URL,
        BrowserContract.SuggestedSites.TITLE,
        BrowserContract.Combined.HISTORY_ID
    };

    private static final String JSON_KEY_URL = "url";
    private static final String JSON_KEY_TITLE = "title";
    private static final String JSON_KEY_IMAGE_URL = "imageurl";
    private static final String JSON_KEY_BG_COLOR = "bgcolor";
    private static final String JSON_KEY_RESTRICTED = "restricted";

    private static class Site {
        public final String url;
        public final String title;
        public final String imageUrl;
        public final String bgColor;
        public final boolean restricted;

        public Site(JSONObject json) throws JSONException {
            this.restricted =  !json.isNull(JSON_KEY_RESTRICTED);
            this.url = json.getString(JSON_KEY_URL);
            this.title = json.getString(JSON_KEY_TITLE);
            this.imageUrl = json.getString(JSON_KEY_IMAGE_URL);
            this.bgColor = json.getString(JSON_KEY_BG_COLOR);

            validate();
        }

        public Site(String url, String title, String imageUrl, String bgColor) {
            this.url = url;
            this.title = title;
            this.imageUrl = imageUrl;
            this.bgColor = bgColor;
            this.restricted = false;

            validate();
        }

        private void validate() {
            // Site instances must have non-empty values for all properties except IDs.
            if (TextUtils.isEmpty(url) ||
                TextUtils.isEmpty(title) ||
                TextUtils.isEmpty(imageUrl) ||
                TextUtils.isEmpty(bgColor)) {
                throw new IllegalStateException("Suggested sites must have a URL, title, " +
                                                "image URL, and background color.");
            }
        }

        @Override
        public String toString() {
            return "{ url = " + url + "\n" +
                     "restricted = " + restricted + "\n" +
                     "title = " + title + "\n" +
                     "imageUrl = " + imageUrl + "\n" +
                     "bgColor = " + bgColor + " }";
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            if (restricted) {
                json.put(JSON_KEY_RESTRICTED, true);
            }

            json.put(JSON_KEY_URL, url);
            json.put(JSON_KEY_TITLE, title);
            json.put(JSON_KEY_IMAGE_URL, imageUrl);
            json.put(JSON_KEY_BG_COLOR, bgColor);

            return json;
        }
    }

    final Context context;
    final Distribution distribution;
    private File cachedFile;
    private Map<String, Site> cachedSites;
    private Set<String> cachedBlacklist;

    public SuggestedSites(Context appContext) {
        this(appContext, null);
    }

    public SuggestedSites(Context appContext, Distribution distribution) {
        this(appContext, distribution, null);
    }

    public SuggestedSites(Context appContext, Distribution distribution, File file) {
        this.context = appContext;
        this.distribution = distribution;
        this.cachedFile = file;
    }

    synchronized File getFile() {
        if (cachedFile == null) {
            cachedFile = GeckoProfile.get(context).getFile(FILENAME);
        }
        return cachedFile;
    }

    private static boolean isNewLocale(Context context, Locale requestedLocale) {
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);

        String locale = prefs.getString(PREF_SUGGESTED_SITES_LOCALE_OLD, null);
        if (locale != null) {
          // Migrate the old pref and remove it
          final Editor editor = prefs.edit();
          editor.remove(PREF_SUGGESTED_SITES_LOCALE_OLD);
          editor.putString(PREF_SUGGESTED_SITES_LOCALE, locale);
          editor.apply();
        } else {
          locale = prefs.getString(PREF_SUGGESTED_SITES_LOCALE, null);
        }
        if (locale == null) {
            // Initialize config with the current locale
            updateSuggestedSitesLocale(context);
            return true;
        }

        return !TextUtils.equals(requestedLocale.toString(), locale);
    }

    /**
     * Return the current locale and its fallback (en_US) in order.
     */
    private static List<Locale> getAcceptableLocales() {
        final List<Locale> locales = new ArrayList<Locale>();

        final Locale defaultLocale = Locale.getDefault();
        locales.add(defaultLocale);

        if (!defaultLocale.equals(Locale.US)) {
            locales.add(Locale.US);
        }

        return locales;
    }

    private static Map<String, Site> loadSites(File f) throws IOException {
        Scanner scanner = null;

        try {
            scanner = new Scanner(f, "UTF-8");
            return loadSites(scanner.useDelimiter("\\A").next());
        } finally {
            if (scanner != null) {
                scanner.close();
            }
        }
    }

    private static Map<String, Site> loadSites(String jsonString) {
        if (TextUtils.isEmpty(jsonString)) {
            return null;
        }

        Map<String, Site> sites = null;

        try {
            final JSONArray jsonSites = new JSONArray(jsonString);
            sites = new LinkedHashMap<String, Site>(jsonSites.length());

            final int count = jsonSites.length();
            for (int i = 0; i < count; i++) {
                final Site site = new Site(jsonSites.getJSONObject(i));
                sites.put(site.url, site);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to refresh suggested sites", e);
            return null;
        }

        return sites;
    }

    /**
     * Saves suggested sites file to disk. Access to this method should
     * be synchronized on 'file'.
     */
    static void saveSites(File f, Map<String, Site> sites) {
        ThreadUtils.assertNotOnUiThread();

        if (sites == null || sites.isEmpty()) {
            return;
        }

        OutputStreamWriter outputStreamWriter = null;

        try {
            final JSONArray jsonSites = new JSONArray();
            for (Site site : sites.values()) {
                jsonSites.put(site.toJSON());
            }

            outputStreamWriter = new OutputStreamWriter(new FileOutputStream(f), StringUtils.UTF_8);

            final String jsonString = jsonSites.toString();
            outputStreamWriter.write(jsonString, 0, jsonString.length());
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to save suggested sites", e);
        } finally {
            IOUtils.safeStreamClose(outputStreamWriter);
        }
    }

    private void maybeWaitForDistribution() {
        if (distribution == null) {
            return;
        }

        distribution.addOnDistributionReadyCallback(new Distribution.ReadyCallback() {
            @Override
            public void distributionNotFound() {
                // If distribution doesn't exist, simply continue to load
                // suggested sites directly from resources. See refresh().
            }

            @Override
            public void distributionFound(Distribution distribution) {
                Log.d(LOGTAG, "Running post-distribution task: suggested sites.");
                // Merge suggested sites from distribution with the
                // default ones. Distribution takes precedence.
                Map<String, Site> sites = loadFromDistribution(distribution);
                if (sites == null) {
                    sites = new LinkedHashMap<String, Site>();
                }
                sites.putAll(loadFromResource());

                // Update cached list of sites.
                setCachedSites(sites);

                // Save the result to disk.
                final File file = getFile();
                synchronized (file) {
                    saveSites(file, sites);
                }

                // Then notify any active loaders about the changes.
                final ContentResolver cr = context.getContentResolver();
                cr.notifyChange(BrowserContract.SuggestedSites.CONTENT_URI, null);
            }

            @Override
            public void distributionArrivedLate(Distribution distribution) {
                distributionFound(distribution);
            }
        });
    }

    /**
     * Loads suggested sites from a distribution file either matching the
     * current locale or with the fallback locale (en-US).
     *
     * It's assumed that the given distribution instance is ready to be
     * used and exists.
     */
    static Map<String, Site> loadFromDistribution(Distribution dist) {
        for (Locale locale : getAcceptableLocales()) {
            try {
                final String languageTag = Locales.getLanguageTag(locale);
                final String path = String.format("suggestedsites/locales/%s/%s",
                                                  languageTag, FILENAME);

                final File f = dist.getDistributionFile(path);
                if (f == null) {
                    Log.d(LOGTAG, "No suggested sites for locale: " + languageTag);
                    continue;
                }

                return loadSites(f);
            } catch (Exception e) {
                Log.e(LOGTAG, "Failed to open suggested sites for locale " +
                              locale + " in distribution.", e);
            }
        }

        return null;
    }

    private Map<String, Site> loadFromProfile() {
        try {
            final File file = getFile();
            synchronized (file) {
                return loadSites(file);
            }
        } catch (FileNotFoundException e) {
            maybeWaitForDistribution();
        } catch (IOException e) {
            // Fall through, return null.
        }

        return null;
    }

    Map<String, Site> loadFromResource() {
        try {
            return loadSites(RawResource.getAsString(context, R.raw.suggestedsites));
        } catch (IOException e) {
            return null;
        }
    }

    private synchronized void setCachedSites(Map<String, Site> sites) {
        cachedSites = Collections.unmodifiableMap(sites);
        updateSuggestedSitesLocale(context);
    }

    /**
     * Refreshes the cached list of sites either from the default raw
     * source or standard file location. This will be called on every
     * cache miss during a {@code get()} call.
     */
    private void refresh() {
        Log.d(LOGTAG, "Refreshing suggested sites from file");

        Map<String, Site> sites = loadFromProfile();
        if (sites == null) {
            sites = loadFromResource();
        }

        // Update cached list of sites.
        if (sites != null) {
            setCachedSites(sites);
        }
    }

    private static void updateSuggestedSitesLocale(Context context) {
        final Editor editor = GeckoSharedPrefs.forProfile(context).edit();
        editor.putString(PREF_SUGGESTED_SITES_LOCALE, Locale.getDefault().toString());
        editor.apply();
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
        final boolean isNewLocale = isNewLocale(context, locale);

        // Force the suggested sites file in profile dir to be re-generated
        // if the locale has changed.
        if (isNewLocale) {
            getFile().delete();
        }

        if (cachedSites == null || isNewLocale) {
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
        // History IDS: real history is positive, -1 is no history id in the combined table
        // hence we can start at -2 for suggested sites
        int id = -1;
        for (Site site : cachedSites.values()) {
            // Decrement ID here: this ensure we have a consistent ID to URL mapping, even if items
            // are removed. If we instead decremented at the point of insertion we'd end up with
            // ID conflicts when a suggested site is removed. (note that cachedSites does not change
            // while we're already showing topsites)
            --id;
            if (cursor.getCount() == maxCount) {
                break;
            }

            if (excludeUrls != null && excludeUrls.contains(site.url)) {
                continue;
            }

            final boolean restrictedProfile =  Restrictions.isRestrictedProfile(context);

            if (restrictedProfile == site.restricted) {
                final RowBuilder row = cursor.newRow();
                row.add(id);
                row.add(site.url);
                row.add(site.title);
                row.add(id);
            }
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

        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        String sitesString = prefs.getString(PREF_SUGGESTED_SITES_HIDDEN_OLD, null);
        if (sitesString != null) {
          // Migrate the old pref and remove it
          final Editor editor = prefs.edit();
          editor.remove(PREF_SUGGESTED_SITES_HIDDEN_OLD);
          editor.putString(PREF_SUGGESTED_SITES_HIDDEN, sitesString);
          editor.apply();
        } else {
          sitesString = prefs.getString(PREF_SUGGESTED_SITES_HIDDEN, null);
        }

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
        prefs.edit().putString(PREF_SUGGESTED_SITES_HIDDEN, siteString).apply();
    }
}
