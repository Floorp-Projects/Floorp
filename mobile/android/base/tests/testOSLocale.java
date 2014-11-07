/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.Locale;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.PrefsHelper;

import android.content.SharedPreferences;


public class testOSLocale extends BaseTest {
    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Clear per-profile SharedPreferences as a workaround for Bug 1069687.
        // We're trying to exercise logic that only applies on first onCreate!
        // We can't rely on this occurring prior to the first broadcast, though,
        // so see the main test method for more logic.
        final String profileName = getTestProfile().getName();
        mAsserter.info("Setup", "Clearing pref in " + profileName + ".");
        GeckoSharedPrefs.forProfileName(getActivity(), profileName)
                        .edit()
                        .remove("osLocale")
                        .apply();
    }

    public static class PrefState extends PrefsHelper.PrefHandlerBase {
        private static final String PREF_LOCALE_OS = "intl.locale.os";
        private static final String PREF_ACCEPT_LANG = "intl.accept_languages";

        private static final String[] TO_FETCH = {PREF_LOCALE_OS, PREF_ACCEPT_LANG};

        public volatile String osLocale;
        public volatile String acceptLanguages;

        private final Object waiter = new Object();

        public void fetch() throws InterruptedException {
            // Wait for any pending changes to have taken. Bug 1092580.
            GeckoAppShell.sendEventToGeckoSync(GeckoEvent.createNoOpEvent());
            synchronized (waiter) {
                PrefsHelper.getPrefs(TO_FETCH, this);
                waiter.wait(MAX_WAIT_MS);
            }
        }

        @Override
        public void prefValue(String pref, String value) {
            switch (pref) {
            case PREF_LOCALE_OS:
                osLocale = value;
                return;
            case PREF_ACCEPT_LANG:
                acceptLanguages = value;
                return;
            }
        }

        @Override
        public void finish() {
            synchronized (waiter) {
                waiter.notify();
            }
        }
    }

    public void testOSLocale() throws Exception {
        blockForDelayedStartup();

        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(getActivity());
        final PrefState state = new PrefState();

        state.fetch();

        // We don't know at this point whether we were run against a dirty profile or not.
        //
        // If we cleared the pref above prior to BrowserApp's delayed init, or our Gecko
        // profile has been used before, then we're already going to be set up for en-US.
        //
        // If we cleared the pref after the initial broadcast, and our Android-side profile
        // has been used before but the Gecko profile is clean, then the Gecko prefs won't
        // have been set.
        //
        // Instead, we always send a new locale code, and see what we get.
        final Locale fr = BrowserLocaleManager.parseLocaleCode("fr");
        BrowserLocaleManager.storeAndNotifyOSLocale(prefs, fr);

        state.fetch();

        mAsserter.is(state.osLocale, "fr", "We're in fr.");

        // Now we can see what the expected Accept-Languages header should be.
        // The OS locale is 'fr', so we have our app locale (en-US),
        // the OS locale (fr), then any remaining fallbacks from intl.properties.
        mAsserter.is(state.acceptLanguages, "en-us,fr,en", "We have the default en-US+fr Accept-Languages.");

        // Now set the app locale to be es-ES.
        BrowserLocaleManager.getInstance().setSelectedLocale(getActivity(), "es-ES");

        state.fetch();

        mAsserter.is(state.osLocale, "fr", "We're still in fr.");

        // The correct set here depends on whether the
        // browser was built with multiple locales or not.
        // This is exasperating, but hey.
        final boolean isMultiLocaleBuild = false;

        // This never changes.
        final String SELECTED_LOCALES = "es-es,fr,";

        // Expected, from es-ES's intl.properties:
        final String EXPECTED = SELECTED_LOCALES +
                                (isMultiLocaleBuild ? "es,en-us,en" :  // Expected, from es-ES's intl.properties.
                                                      "en-us,en");     // Expected, from en-US (the default).

        mAsserter.is(state.acceptLanguages, EXPECTED, "We have the right es-ES+fr Accept-Languages for this build.");

        // And back to en-US.
        final Locale en_US = BrowserLocaleManager.parseLocaleCode("en-US");
        BrowserLocaleManager.storeAndNotifyOSLocale(prefs, en_US);
        BrowserLocaleManager.getInstance().resetToSystemLocale(getActivity());

        state.fetch();

        mAsserter.is(state.osLocale, "en-US", "We're in en-US.");
        mAsserter.is(state.acceptLanguages, "en-us,en", "We have the default processed en-US Accept-Languages.");
    }
}