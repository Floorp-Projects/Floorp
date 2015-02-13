/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.util.Locale;
import java.util.jar.JarInputStream;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.SuggestedSites;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.distribution.ReferrerDescriptor;
import org.mozilla.gecko.distribution.ReferrerReceiver;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

/**
 * Tests distribution customization.
 * mock-package.zip should contain the following directory structure:
 *
 *   distribution/
 *     preferences.json
 *     bookmarks.json
 *     searchplugins/
 *       common/
 *         engine.xml
 *     suggestedsites/
 *       locales/
 *         en-US/
 *           suggestedsites.json
 */
public class testDistribution extends ContentProviderTest {
    private static final String CLASS_REFERRER_RECEIVER = "org.mozilla.gecko.distribution.ReferrerReceiver";
    private static final String ACTION_INSTALL_REFERRER = "com.android.vending.INSTALL_REFERRER";
    private static final int WAIT_TIMEOUT_MSEC = 10000;
    public static final String LOGTAG = "GeckoTestDistribution";

    public static class TestableDistribution extends Distribution {
        @Override
        protected JarInputStream fetchDistribution(URI uri,
                HttpURLConnection connection) throws IOException {
            Log.i(LOGTAG, "Not downloading: this is a test.");
            return null;
        }

        public TestableDistribution(Context context) {
            super(context);
        }

        public void go() {
            doInit();
        }

        public static void clearReferrerDescriptorForTesting() {
            referrer = null;
        }

        public static ReferrerDescriptor getReferrerDescriptorForTesting() {
            return referrer;
        }
    }

    private static final String MOCK_PACKAGE = "mock-package.zip";
    private static final int PREF_REQUEST_ID = 0x7357;

    private Activity mActivity;

    /**
     * This is a hack.
     *
     * Startup results in us writing prefs -- we fetch the Distribution, which
     * caches its state. Our tests try to wipe those prefs, but apparently
     * sometimes race with startup, which leads to us not getting one of our
     * expected messages. The test fails.
     *
     * This hack waits for any existing background tasks -- such as the one that
     * writes prefs -- to finish before we begin the test.
     */
    private void waitForBackgroundHappiness() {
        final Object signal = new Object();
        final Runnable done = new Runnable() {
            @Override
            public void run() {
                synchronized (signal) {
                    signal.notify();
                }
            }
        };
        synchronized (signal) {
            ThreadUtils.postToBackgroundThread(done);
            try {
                signal.wait();
            } catch (InterruptedException e) {
                mAsserter.ok(false, "InterruptedException waiting on background thread.", e.toString());
            }
        }
        mAsserter.dumpLog("Background task completed. Proceeding.");
    }

    public void testDistribution() throws Exception {
        mActivity = getActivity();

        String mockPackagePath = getMockPackagePath();

        // Wait for any startup-related background distribution shenanigans to
        // finish. This reduces the chance of us racing with startup pref writes.
        waitForBackgroundHappiness();

        // Pre-clear distribution pref, override suggested sites and run tiles tests.
        clearDistributionPref();
        Distribution dist = initDistribution(mockPackagePath);
        SuggestedSites suggestedSites = new SuggestedSites(mActivity, dist);
        GeckoProfile.get(mActivity).getDB().setSuggestedSites(suggestedSites);

        // Test tiles uploading for an en-US OS locale with no app locale.
        setOSLocale(Locale.US);
        checkTilesReporting("en-US");

        // Test tiles uploading for an es-MX OS locale with no app locale.
        setOSLocale(new Locale("es", "MX"));
        checkTilesReporting("es-MX");

        // Pre-clear distribution pref, run basic preferences and en-US localized preferences Tests
        clearDistributionPref();
        setTestLocale("en-US");
        initDistribution(mockPackagePath);
        checkPreferences();
        checkLocalizedPreferences("en-US");
        checkSearchPlugin();

        // Pre-clear distribution pref, and run es-MX localized preferences Test
        clearDistributionPref();
        setTestLocale("es-MX");
        initDistribution(mockPackagePath);
        checkLocalizedPreferences("es-MX");

        // Test the (stubbed) download interaction.
        setTestLocale("en-US");
        clearDistributionPref();
        doTestValidReferrerIntent();

        clearDistributionPref();
        doTestInvalidReferrerIntent();
    }

    private void setOSLocale(Locale locale) {
        Locale.setDefault(locale);
        BrowserLocaleManager.storeAndNotifyOSLocale(GeckoSharedPrefs.forProfile(mActivity), locale);
    }

    private abstract class ExpectNoDistributionCallback implements Distribution.ReadyCallback {
            @Override
            public void distributionFound(final Distribution distribution) {
                mAsserter.ok(false, "No distributionFound.", "Wasn't expecting a distribution!");
                synchronized (distribution) {
                    distribution.notifyAll();
                }
            }

            @Override
            public void distributionArrivedLate(final Distribution distribution) {
                mAsserter.ok(false, "No distributionArrivedLate.", "Wasn't expecting a late distribution!");
            }
    }

    private void doReferrerTest(String ref, final TestableDistribution distribution, final Distribution.ReadyCallback distributionReady) throws InterruptedException {
        final Intent intent = new Intent(ACTION_INSTALL_REFERRER);
        intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, CLASS_REFERRER_RECEIVER);
        intent.putExtra("referrer", ref);

        final BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Log.i(LOGTAG, "Test received " + intent.getAction());

                ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        distribution.addOnDistributionReadyCallback(distributionReady);
                        distribution.go();
                    }
                });
            }
        };

        IntentFilter intentFilter = new IntentFilter(ReferrerReceiver.ACTION_REFERRER_RECEIVED);
        final LocalBroadcastManager localBroadcastManager = LocalBroadcastManager.getInstance(mActivity);
        localBroadcastManager.registerReceiver(receiver, intentFilter);

        Log.i(LOGTAG, "Broadcasting referrer intent.");
        try {
            mActivity.sendBroadcast(intent, null);
            synchronized (distribution) {
                distribution.wait(WAIT_TIMEOUT_MSEC);
            }
        } finally {
            localBroadcastManager.unregisterReceiver(receiver);
        }
    }

    public void doTestValidReferrerIntent() throws Exception {
        // Equivalent to
        // am broadcast -a com.android.vending.INSTALL_REFERRER \
        //              -n org.mozilla.fennec/org.mozilla.gecko.distribution.ReferrerReceiver \
        //              --es "referrer" "utm_source=mozilla&utm_medium=testmedium&utm_term=testterm&utm_content=testcontent&utm_campaign=distribution"
        final String ref = "utm_source=mozilla&utm_medium=testmedium&utm_term=testterm&utm_content=testcontent&utm_campaign=distribution";
        final TestableDistribution distribution = new TestableDistribution(mActivity);
        final Distribution.ReadyCallback distributionReady = new ExpectNoDistributionCallback() {
            @Override
            public void distributionNotFound() {
                Log.i(LOGTAG, "Test told distribution processing is done.");
                mAsserter.ok(!distribution.exists(), "Not processed.", "No download because we're offline.");
                ReferrerDescriptor referrerValue = TestableDistribution.getReferrerDescriptorForTesting();
                mAsserter.dumpLog("Referrer was " + referrerValue);
                mAsserter.is(referrerValue.content, "testcontent", "Referrer content");
                mAsserter.is(referrerValue.medium, "testmedium", "Referrer medium");
                mAsserter.is(referrerValue.campaign, "distribution", "Referrer campaign");
                synchronized (distribution) {
                    distribution.notifyAll();
                }
            }
        };

        doReferrerTest(ref, distribution, distributionReady);
    }

    /**
     * Test processing if the campaign isn't "distribution". The intent shouldn't
     * result in a download, and won't be saved as the temporary referrer,
     * even if we *do* include it in a Campaign:Set message.
     */
    public void doTestInvalidReferrerIntent() throws Exception {
        // Equivalent to
        // am broadcast -a com.android.vending.INSTALL_REFERRER \
        //              -n org.mozilla.fennec/org.mozilla.gecko.distribution.ReferrerReceiver \
        //              --es "referrer" "utm_source=mozilla&utm_medium=testmedium&utm_term=testterm&utm_content=testcontent&utm_campaign=testname"
        final String ref = "utm_source=mozilla&utm_medium=testmedium&utm_term=testterm&utm_content=testcontent&utm_campaign=testname";
        final TestableDistribution distribution = new TestableDistribution(mActivity);
        final Distribution.ReadyCallback distributionReady = new ExpectNoDistributionCallback() {
            @Override
            public void distributionNotFound() {
                mAsserter.ok(!distribution.exists(), "Not processed.", "No download because campaign was wrong.");
                ReferrerDescriptor referrerValue = TestableDistribution.getReferrerDescriptorForTesting();
                mAsserter.is(referrerValue, null, "No referrer.");
                synchronized (distribution) {
                    distribution.notifyAll();
                }
            }
        };

        doReferrerTest(ref, distribution, distributionReady);
    }

    // Initialize the distribution from the mock package.
    private Distribution initDistribution(String aPackagePath) {
        // Call Distribution.init with the mock package.
        Actions.EventExpecter distributionSetExpecter = mActions.expectGeckoEvent("Distribution:Set:OK");
        Distribution dist = Distribution.init(mActivity, aPackagePath, "prefs-" + System.currentTimeMillis());
        distributionSetExpecter.blockForEvent();
        distributionSetExpecter.unregisterListener();
        return dist;
    }

    // Test distribution and preferences values stored in preferences.json
    private void checkPreferences() {
        String prefID = "distribution.id";
        String prefAbout = "distribution.about";
        String prefVersion = "distribution.version";
        String prefTestBoolean = "distribution.test.boolean";
        String prefTestString = "distribution.test.string";
        String prefTestInt = "distribution.test.int";

        try {
            final String[] prefNames = { prefID,
                                         prefAbout,
                                         prefVersion,
                                         prefTestBoolean,
                                         prefTestString,
                                         prefTestInt };

            Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
            mActions.sendPreferencesGetEvent(PREF_REQUEST_ID, prefNames);

            JSONObject data = null;
            int requestId = -1;

            // Wait until we get the correct "Preferences:Data" event
            while (requestId != PREF_REQUEST_ID) {
                data = new JSONObject(eventExpecter.blockForEventData());
                requestId = data.getInt("requestId");
            }
            eventExpecter.unregisterListener();

            JSONArray preferences = data.getJSONArray("preferences");
            for (int i = 0; i < preferences.length(); i++) {
                JSONObject pref = (JSONObject) preferences.get(i);
                String name = pref.getString("name");

                if (name.equals(prefID)) {
                    mAsserter.is(pref.getString("value"), "test-partner", "check " + prefID);
                } else if (name.equals(prefAbout)) {
                    mAsserter.is(pref.getString("value"), "Test Partner", "check " + prefAbout);
                } else if (name.equals(prefVersion)) {
                    mAsserter.is(pref.getInt("value"), 1, "check " + prefVersion);
                } else if (name.equals(prefTestBoolean)) {
                    mAsserter.is(pref.getBoolean("value"), true, "check " + prefTestBoolean);
                } else if (name.equals(prefTestString)) {
                    mAsserter.is(pref.getString("value"), "test", "check " + prefTestString);
                } else if (name.equals(prefTestInt)) {
                    mAsserter.is(pref.getInt("value"), 5, "check " + prefTestInt);
                }
            }

        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting preferences", e.toString());
        }
    }

    private void checkSearchPlugin() {
        Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("SearchEngines:Data");
        mActions.sendGeckoEvent("SearchEngines:GetVisible", null);

        try {
            JSONObject data = new JSONObject(eventExpecter.blockForEventData());
            eventExpecter.unregisterListener();
            JSONArray searchEngines = data.getJSONArray("searchEngines");
            boolean foundEngine = false;
            for (int i = 0; i < searchEngines.length(); i++) {
                JSONObject engine = (JSONObject) searchEngines.get(i);
                String name = engine.getString("name");
                if (name.equals("Test search engine")) {
                    foundEngine = true;
                    break;
                }
            }
            mAsserter.ok(foundEngine, "check search plugin", "found test search plugin");
        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting search plugins", e.toString());
        }
    }

    // Sets the distribution locale preference for the test.
    private void setTestLocale(String locale) {
        BrowserLocaleManager.getInstance().setSelectedLocale(mActivity, locale);
    }

    // Test localized distribution and preferences values stored in preferences.json
    private void checkLocalizedPreferences(String aLocale) {
        String prefAbout = "distribution.about";
        String prefLocalizeable = "distribution.test.localizeable";
        String prefLocalizeableOverride = "distribution.test.localizeable-override";

        try {
            final String[] prefNames = { prefAbout, prefLocalizeable, prefLocalizeableOverride };

            Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
            mActions.sendPreferencesGetEvent(PREF_REQUEST_ID, prefNames);

            JSONObject data = null;
            int requestId = -1;

            // Wait until we get the correct "Preferences:Data" event
            while (requestId != PREF_REQUEST_ID) {
                data = new JSONObject(eventExpecter.blockForEventData());
                requestId = data.getInt("requestId");
            }
            eventExpecter.unregisterListener();

            JSONArray preferences = data.getJSONArray("preferences");
            for (int i = 0; i < preferences.length(); i++) {
                JSONObject pref = (JSONObject) preferences.get(i);
                String name = pref.getString("name");

                if (name.equals(prefAbout)) {
                    if (aLocale.equals("en-US")) {
                        mAsserter.is(pref.getString("value"), "Test Partner", "check " + prefAbout);
                    } else if (aLocale.equals("es-MX")) {
                        mAsserter.is(pref.getString("value"), "Afiliado de Prueba", "check " + prefAbout);
                    }
                } else if (name.equals(prefLocalizeable)) {
                    if (aLocale.equals("en-US")) {
                        mAsserter.is(pref.getString("value"), "http://test.org/en-US/en-US/", "check " + prefLocalizeable);
                    } else if (aLocale.equals("es-MX")) {
                        mAsserter.is(pref.getString("value"), "http://test.org/es-MX/es-MX/", "check " + prefLocalizeable);
                    }
                } else if (name.equals(prefLocalizeableOverride)) {
                    if (aLocale.equals("en-US")) {
                        mAsserter.is(pref.getString("value"), "http://cheese.com", "check " + prefLocalizeableOverride);
                    } else if (aLocale.equals("es-MX")) {
                        mAsserter.is(pref.getString("value"), "http://test.org/es-MX/", "check " + prefLocalizeableOverride);
                    }
                }
            }

        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting preferences", e.toString());
        }
    }

    // Copies the mock package to the data directory and returns the file path to it.
    private String getMockPackagePath() {
        String mockPackagePath = "";

        try {
            InputStream inStream = getAsset(MOCK_PACKAGE);
            File dataDir = new File(mActivity.getApplicationInfo().dataDir);
            File outFile = new File(dataDir, MOCK_PACKAGE);

            OutputStream outStream = new FileOutputStream(outFile);
            int b;
            while ((b = inStream.read()) != -1) {
                outStream.write(b);
            }
            inStream.close();
            outStream.close();

            mockPackagePath = outFile.getPath();

        } catch (Exception e) {
            mAsserter.ok(false, "exception copying mock distribution package to data directory", e.toString());
        }

        return mockPackagePath;
    }

    /**
     * Clears the distribution pref to return distribution state to STATE_UNKNOWN,
     * and wipes the in-memory referrer pigeonhole.
     */
    private void clearDistributionPref() {
        mAsserter.dumpLog("Clearing distribution pref.");
        SharedPreferences settings = mActivity.getSharedPreferences("GeckoApp", Activity.MODE_PRIVATE);
        String keyName = mActivity.getPackageName() + ".distribution_state";
        settings.edit().remove(keyName).commit();
        TestableDistribution.clearReferrerDescriptorForTesting();
    }

    public void checkTilesReporting(String localeCode) throws JSONException {
        // Slight hack: Force top sites grid to reload.
        inputAndLoadUrl(StringHelper.ABOUT_BLANK_URL);
        inputAndLoadUrl(StringHelper.ABOUT_HOME_URL);

        // Click the first tracking tile and verify the posted data.
        JSONObject response = clickTrackingTile(StringHelper.DISTRIBUTION1_LABEL);
        mAsserter.is(response.getInt("click"), 0, "JSON click index matched");
        mAsserter.is(response.getString("locale"), localeCode, "JSON locale code matched");
        mAsserter.is(response.getString("tiles"), "[{\"id\":123},{\"id\":456},{\"id\":632},{\"id\":629},{\"id\":630},{\"id\":631}]", "JSON tiles data matched");

        inputAndLoadUrl(StringHelper.ABOUT_HOME_URL);

        // Pin the second tracking tile.
        pinTopSite(StringHelper.DISTRIBUTION2_LABEL);

        // Click the second tracking tile and verify the posted data.
        response = clickTrackingTile(StringHelper.DISTRIBUTION2_LABEL);
        mAsserter.is(response.getInt("click"), 1, "JSON click index matched");
        mAsserter.is(response.getString("tiles"), "[{\"id\":123},{\"id\":456,\"pin\":true},{\"id\":632},{\"id\":629},{\"id\":630},{\"id\":631}]", "JSON tiles data matched");

        inputAndLoadUrl(StringHelper.ABOUT_HOME_URL);

        // Unpin the second tracking tile.
        unpinTopSite(StringHelper.DISTRIBUTION2_LABEL);
    }

    private JSONObject clickTrackingTile(String text) throws JSONException {
        boolean tileFound = waitForText(text);
        mAsserter.ok(tileFound, "Found tile: " + text, null);

        Actions.EventExpecter loadExpecter = mActions.expectGeckoEvent("Robocop:TilesResponse");
        mSolo.clickOnText(text);
        String data = loadExpecter.blockForEventData();
        JSONObject dataJSON = new JSONObject(data);
        String response = dataJSON.getString("response");
        return new JSONObject(response);
    }

    @Override
    public void setUp() throws Exception {
        // TODO: Set up the content provider after setting the distribution.
        super.setUp(sBrowserProviderCallable, BrowserContract.AUTHORITY, "browser.db");
    }

    private void delete(File file) throws Exception {
      if (file.isDirectory()) {
        File[] files = file.listFiles();
        for (File f : files) {
          delete(f);
        }
      }
      mAsserter.ok(file.delete(), "clean up distribution files", "deleted " + file.getPath());
    }

    @Override
    public void tearDown() throws Exception {
        File dataDir = new File(mActivity.getApplicationInfo().dataDir);

        // Delete mock package from data directory.
        File mockPackage = new File(dataDir, MOCK_PACKAGE);
        mAsserter.ok(mockPackage.delete(), "clean up mock package", "deleted " + mockPackage.getPath());

        // Recursively delete distribution files that Distribution.init copied to data directory.
        File distDir = new File(dataDir, "distribution");
        delete(distDir);

        clearDistributionPref();

        super.tearDown();
    }
}
