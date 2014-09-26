/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import org.json.JSONArray;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.preferences.GeckoPreferences;

import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.SystemClock;
import android.test.RenamingDelegatingContext;
import android.test.mock.MockResources;

public class TestSuggestedSites extends BrowserTestCase {
    private static class TestContext extends RenamingDelegatingContext {
        private static final String PREFIX = "TestSuggestedSites-";

        private final Resources resources;
        private final Set<String> usedPrefs;

        public TestContext(Context context) {
            super(context, PREFIX);
            resources = new TestResources();
            usedPrefs = Collections.synchronizedSet(new HashSet<String>());
        }

        @Override
        public Resources getResources() {
            return resources;
        }

        @Override
        public SharedPreferences getSharedPreferences(String name, int mode) {
            usedPrefs.add(name);
            return super.getSharedPreferences(PREFIX + name, mode);
        }

        public void clearUsedPrefs() {
            for (String prefsName : usedPrefs) {
                getSharedPreferences(prefsName, 0).edit().clear().commit();
            }

            usedPrefs.clear();
        }
    }

    private static class TestResources extends MockResources {
        private String suggestedSites;

        @Override
        public InputStream openRawResource(int id) {
            if (id == R.raw.suggestedsites && suggestedSites != null) {
                return new ByteArrayInputStream(suggestedSites.getBytes());
            }

            return null;
        }

        public void setSuggestedSitesResource(String suggestedSites) {
            this.suggestedSites = suggestedSites;
        }
    }

    private static class TestDistribution extends Distribution {
        private final Map<Locale, File> filesPerLocale;

        public TestDistribution(Context context) {
            super(context);
            this.filesPerLocale = new HashMap<Locale, File>();
        }

        @Override
        public File getDistributionFile(String name) {
            for (Locale locale : filesPerLocale.keySet()) {
                if (name.startsWith("suggestedsites/locales/" + BrowserLocaleManager.getLanguageTag(locale))) {
                    return filesPerLocale.get(locale);
                }
            }

            return null;
        }

        @Override
        public boolean exists() {
            return true;
        }

        public void setFileForLocale(Locale locale, File file) {
            filesPerLocale.put(locale, file);
        }

        public void start() {
            doInit();
        }
    }

    class TestObserver extends ContentObserver {
        private final Object changeLock;

        public TestObserver(Object changeLock) {
            super(null);
            this.changeLock = changeLock;
        }

        @Override
        public void onChange(boolean selfChange) {
            synchronized(changeLock) {
                changeLock.notifyAll();
            }
        }
    }

    private static final int DEFAULT_LIMIT = 6;

    private static final String DIST_PREFIX = "dist";

    private TestContext context;
    private TestResources resources;
    private List<File> tempFiles;

    private String generateSites(int n) {
        return generateSites(n, "");
    }

    private String generateSites(int n, String prefix) {
        JSONArray sites = new JSONArray();

        try {
            for (int i = 0; i < n; i++) {
                JSONObject site = new JSONObject();
                site.put("url", prefix + "url" + i);
                site.put("title", prefix + "title" + i);
                site.put("imageurl", prefix + "imageUrl" + i);
                site.put("bgcolor", prefix + "bgColor" + i);

                sites.put(site);
            }
        } catch (Exception e) {
            return "";
        }

        return sites.toString();
    }

    private File createDistSuggestedSitesFile(int n) {
        FileOutputStream fos = null;

        try {
            File distFile = File.createTempFile("distrosites", ".json",
                                                context.getCacheDir());

            fos = new FileOutputStream(distFile);
            fos.write(generateSites(n, DIST_PREFIX).getBytes());

            return distFile;
        } catch (IOException e) {
            fail("Failed to create temp suggested sites file");
        } finally {
            if (fos != null) {
                try {
                    fos.close();
                } catch (IOException e) {
                    // Ignore.
                }
            }
        }

        return null;
    }

    private void checkCursorCount(String content, int expectedCount) {
        checkCursorCount(content, expectedCount, DEFAULT_LIMIT);
    }

    private void checkCursorCount(String content, int expectedCount, int limit) {
        resources.setSuggestedSitesResource(content);
        Cursor c = new SuggestedSites(context).get(limit);
        assertEquals(expectedCount, c.getCount());
        c.close();
    }

    protected void setUp() {
        context = new TestContext(getApplicationContext());
        resources = (TestResources) context.getResources();
        tempFiles = new ArrayList<File>();
    }

    protected void tearDown() {
        context.clearUsedPrefs();
        for (File f : tempFiles) {
            f.delete();
        }
    }

    public void testCount() {
        // Empty array = empty cursor
        checkCursorCount(generateSites(0), 0);

        // 2 items = cursor with 2 rows
        checkCursorCount(generateSites(2), 2);

        // 10 items with lower limit = cursor respects limit
        checkCursorCount(generateSites(10), 3, 3);
    }

    public void testEmptyCursor() {
        // Null resource = empty cursor
        checkCursorCount(null, 0);

        // Empty string = empty cursor
        checkCursorCount("", 0);

        // Invalid json string = empty cursor
        checkCursorCount("{ broken: }", 0);
    }

    public void testCursorContent() {
        resources.setSuggestedSitesResource(generateSites(3));

        Cursor c = new SuggestedSites(context).get(DEFAULT_LIMIT);
        assertEquals(3, c.getCount());

        c.moveToPosition(-1);
        while (c.moveToNext()) {
            int position = c.getPosition();

            String url = c.getString(c.getColumnIndexOrThrow(BrowserContract.SuggestedSites.URL));
            assertEquals("url" + position, url);

            String title = c.getString(c.getColumnIndexOrThrow(BrowserContract.SuggestedSites.TITLE));
            assertEquals("title" + position, title);

            String imageUrl = c.getString(c.getColumnIndexOrThrow(BrowserContract.SuggestedSites.IMAGEURL));
            assertEquals("imageUrl" + position, imageUrl);

            String bgColor = c.getString(c.getColumnIndexOrThrow(BrowserContract.SuggestedSites.BGCOLOR));
            assertEquals("bgColor" + position, bgColor);
        }

        c.close();
    }

    public void testExcludeUrls() {
        resources.setSuggestedSitesResource(generateSites(6));

        List<String> excludedUrls = new ArrayList<String>(3);
        excludedUrls.add("url1");
        excludedUrls.add("url3");
        excludedUrls.add("url5");

        List<String> includedUrls = new ArrayList<String>(3);
        includedUrls.add("url0");
        includedUrls.add("url2");
        includedUrls.add("url4");

        Cursor c = new SuggestedSites(context).get(DEFAULT_LIMIT, excludedUrls);

        c.moveToPosition(-1);
        while (c.moveToNext()) {
            String url = c.getString(c.getColumnIndexOrThrow(BrowserContract.SuggestedSites.URL));
            assertFalse(excludedUrls.contains(url));
            assertTrue(includedUrls.contains(url));
        }

        c.close();
    }

    public void testHiddenSites() {
        resources.setSuggestedSitesResource(generateSites(6));

        List<String> visibleUrls = new ArrayList<String>(3);
        visibleUrls.add("url3");
        visibleUrls.add("url4");
        visibleUrls.add("url5");

        List<String> hiddenUrls = new ArrayList<String>(3);
        hiddenUrls.add("url0");
        hiddenUrls.add("url1");
        hiddenUrls.add("url2");

        // Add mocked hidden sites to SharedPreferences.
        StringBuilder hiddenUrlBuilder = new StringBuilder();
        for (String s : hiddenUrls) {
            hiddenUrlBuilder.append(" ");
            hiddenUrlBuilder.append(Uri.encode(s));
        }

        final String hiddenPref = hiddenUrlBuilder.toString();
        GeckoSharedPrefs.forProfile(context).edit()
                                        .putString(SuggestedSites.PREF_SUGGESTED_SITES_HIDDEN, hiddenPref)
                                        .commit();

        Cursor c = new SuggestedSites(context).get(DEFAULT_LIMIT);
        assertEquals(Math.min(3, DEFAULT_LIMIT), c.getCount());

        c.moveToPosition(-1);
        while (c.moveToNext()) {
            String url = c.getString(c.getColumnIndexOrThrow(BrowserContract.SuggestedSites.URL));
            assertFalse(hiddenUrls.contains(url));
            assertTrue(visibleUrls.contains(url));
        }

        c.close();
    }

    public void testDisabledState() {
        resources.setSuggestedSitesResource(generateSites(3));

        Cursor c = new SuggestedSites(context).get(DEFAULT_LIMIT);
        assertEquals(3, c.getCount());
        c.close();

        // Disable suggested sites
        GeckoSharedPrefs.forApp(context).edit()
                                        .putBoolean(GeckoPreferences.PREFS_SUGGESTED_SITES, false)
                                        .commit();

        c = new SuggestedSites(context).get(DEFAULT_LIMIT);
        assertNotNull(c);
        assertEquals(0, c.getCount());
        c.close();
    }

    public void testLocaleChanges() {
        resources.setSuggestedSitesResource(generateSites(3));

        SuggestedSites suggestedSites = new SuggestedSites(context);

        // Initial load with predefined locale
        Cursor c = suggestedSites.get(DEFAULT_LIMIT, Locale.UK);
        assertEquals(3, c.getCount());
        c.close();

        resources.setSuggestedSitesResource(generateSites(5));

        // Second load with same locale should return same results
        // even though the contents of the resource have changed.
        c = suggestedSites.get(DEFAULT_LIMIT, Locale.UK);
        assertEquals(3, c.getCount());
        c.close();

        // Changing the locale forces the cached list to be refreshed.
        c = suggestedSites.get(DEFAULT_LIMIT, Locale.US);
        assertEquals(5, c.getCount());
        c.close();
    }

    public void testDistribution() {
        final int DIST_COUNT = 2;
        final int DEFAULT_COUNT = 3;

        File sitesFile = new File(context.getCacheDir(),
                                  "suggestedsites-" + SystemClock.uptimeMillis() + ".json");
        tempFiles.add(sitesFile);
        assertFalse(sitesFile.exists());

        File distFile = createDistSuggestedSitesFile(DIST_COUNT);
        tempFiles.add(distFile);
        assertTrue(distFile.exists());

        // Init distribution with the mock file.
        TestDistribution distribution = new TestDistribution(context);
        distribution.setFileForLocale(Locale.getDefault(), distFile);
        distribution.start();

        // Init suggested sites with default values.
        resources.setSuggestedSitesResource(generateSites(DEFAULT_COUNT));
        SuggestedSites suggestedSites =
                new SuggestedSites(context, distribution, sitesFile);

        Object changeLock = new Object();

        // Watch for change notifications on suggested sites.
        ContentResolver cr = context.getContentResolver();
        ContentObserver observer = new TestObserver(changeLock);
        cr.registerContentObserver(BrowserContract.SuggestedSites.CONTENT_URI,
                                   false, observer);

        // The initial query will not contain the distribution sites
        // yet. This will happen asynchronously once the distribution
        // is installed.
        Cursor c1 = null;
        try {
            c1 = suggestedSites.get(DEFAULT_LIMIT);
            assertEquals(DEFAULT_COUNT, c1.getCount());
        } finally {
            if (c1 != null) {
                c1.close();
            }
        }

        synchronized(changeLock) {
            try {
                changeLock.wait(5000);
            } catch (InterruptedException ie) {
                fail("No change notification after fetching distribution file");
            }
        }

        // Target file should exist after distribution is deployed.
        assertTrue(sitesFile.exists());
        cr.unregisterContentObserver(observer);

        Cursor c2 = null;
        try {
            c2 = suggestedSites.get(DEFAULT_LIMIT);

            // The next query should contain the distribution contents.
            assertEquals(DIST_COUNT + DEFAULT_COUNT, c2.getCount());

            // The first items should be from the distribution
            for (int i = 0; i < DIST_COUNT; i++) {
                c2.moveToPosition(i);

                String url = c2.getString(c2.getColumnIndexOrThrow(BrowserContract.SuggestedSites.URL));
                assertEquals(DIST_PREFIX +  "url" + i, url);

                String title = c2.getString(c2.getColumnIndexOrThrow(BrowserContract.SuggestedSites.TITLE));
                assertEquals(DIST_PREFIX +  "title" + i, title);
            }

            // The remaining items should be the default ones
            for (int i = 0; i < c2.getCount() - DIST_COUNT; i++) {
                c2.moveToPosition(i + DIST_COUNT);

                String url = c2.getString(c2.getColumnIndexOrThrow(BrowserContract.SuggestedSites.URL));
                assertEquals("url" + i, url);

                String title = c2.getString(c2.getColumnIndexOrThrow(BrowserContract.SuggestedSites.TITLE));
                assertEquals("title" + i, title);
            }
        } finally {
            if (c2 != null) {
                c2.close();
            }
        }
    }
}
