/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;

import org.mozilla.gecko.util.GeckoBundle;

/**
 * Content Blocking API to hold and control anti-tracking, cookie and Safe
 * Browsing settings.
 */
@AnyThread
public class ContentBlocking {
    @AnyThread
    public static class Settings extends RuntimeSettings {
        @AnyThread
        public static class Builder
                extends RuntimeSettings.Builder<Settings> {
            @Override
            protected @NonNull Settings newSettings(final @Nullable Settings settings) {
                return new Settings(settings);
            }

            /**
             * Set content blocking categories.
             *
             * @param cat The categories of resources that should be blocked.
             *            Use one or more of the
             *            {@link ContentBlocking#AT_AD AT_* or SB_*} flags.
             * @return This Builder instance.
             */
            public @NonNull Builder categories(final @Category int cat) {
                getSettings().setCategories(cat);
                return this;
            }

            /**
             * Set cookie storage behavior.
             *
             * @param behavior The storage behavior that should be applied.
             *                 Use one of the {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
             * @return The Builder instance.
             */
            public @NonNull Builder cookieBehavior(final @CookieBehavior int behavior) {
                getSettings().setCookieBehavior(behavior);
                return this;
            }

            /**
             * Set the cookie lifetime.
             *
             * @param lifetime The enforced cookie lifetime.
             *                 Use one of the {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
             * @return The Builder instance.
             */
            public @NonNull Builder cookieLifetime(final @CookieLifetime int lifetime) {
                getSettings().setCookieLifetime(lifetime);
                return this;
            }
        }

        /* package */ final Pref<String> mAt = new Pref<String>(
            "urlclassifier.trackingTable",
            ContentBlocking.catToAtPref(AT_DEFAULT));
        /* package */ final Pref<Boolean> mCm = new Pref<Boolean>(
            "privacy.trackingprotection.cryptomining.enabled", false);
        /* package */ final Pref<String> mCmList = new Pref<String>(
            "urlclassifier.features.cryptomining.blacklistTables",
            ContentBlocking.catToCmListPref(NONE));
        /* package */ final Pref<Boolean> mFp = new Pref<Boolean>(
            "privacy.trackingprotection.fingerprinting.enabled", false);
        /* package */ final Pref<String> mFpList = new Pref<String>(
            "urlclassifier.features.fingerprinting.blacklistTables",
            ContentBlocking.catToFpListPref(NONE));

        /* package */ final Pref<Boolean> mSbMalware = new Pref<Boolean>(
            "browser.safebrowsing.malware.enabled", true);
        /* package */ final Pref<Boolean> mSbPhishing = new Pref<Boolean>(
            "browser.safebrowsing.phishing.enabled", true);
        /* package */ final Pref<Integer> mCookieBehavior = new Pref<Integer>(
            "network.cookie.cookieBehavior", COOKIE_ACCEPT_ALL);
        /* package */ final Pref<Integer> mCookieLifetime = new Pref<Integer>(
            "network.cookie.lifetimePolicy", COOKIE_LIFETIME_NORMAL);

        /**
         * Construct default settings.
         */
        /* package */ Settings() {
            this(null /* settings */);
        }

        /**
         * Copy-construct settings.
         *
         * @param settings Copy from this settings.
         */
        /* package */ Settings(final @Nullable Settings settings) {
            this(null /* parent */, settings);
        }

        /**
         * Copy-construct nested settings.
         *
         * @param parent The parent settings used for nesting.
         * @param settings Copy from this settings.
         */
        /* package */ Settings(final @Nullable RuntimeSettings parent,
                               final @Nullable Settings settings) {
            super(parent);

            if (settings != null) {
                updateSettings(settings);
            }
        }

        private void updateSettings(final @NonNull Settings settings) {
            // We only have pref settings here.
            updatePrefs(settings);
        }

        /**
         * Set content blocking categories.
         *
         * @param cat The categories of resources that should be blocked.
         *            Use one or more of the
         *            {@link ContentBlocking#AT_AD AT_* or SB_*} flags.
         * @return This Settings instance.
         */
        public @NonNull Settings setCategories(final @Category int cat) {
            mAt.commit(ContentBlocking.catToAtPref(cat));

            mCm.commit(ContentBlocking.catToCmPref(cat));
            mCmList.commit(ContentBlocking.catToCmListPref(cat));

            mFp.commit(ContentBlocking.catToFpPref(cat));
            mFpList.commit(ContentBlocking.catToFpListPref(cat));

            mSbMalware.commit(ContentBlocking.catToSbMalware(cat));
            mSbPhishing.commit(ContentBlocking.catToSbPhishing(cat));
            return this;
        }

        /**
         * Get the set content blocking categories.
         *
         * @return The categories of resources to be blocked.
         */
        public @Category int getCategories() {
            return ContentBlocking.atListToCat(mAt.get()) |
                   ContentBlocking.cmListToCat(mCmList.get()) |
                   ContentBlocking.fpListToCat(mFpList.get()) |
                   ContentBlocking.sbMalwareToCat(mSbMalware.get()) |
                   ContentBlocking.sbPhishingToCat(mSbPhishing.get());
        }

        /**
         * Get the assigned cookie storage behavior.
         *
         * @return The assigned behavior, as one of {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
         */
        public @CookieBehavior int getCookieBehavior() {
            return mCookieBehavior.get();
        }

        /**
         * Set cookie storage behavior.
         *
         * @param behavior The storage behavior that should be applied.
         *                 Use one of the {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
         * @return This Settings instance.
         */
        public @NonNull Settings setCookieBehavior(
                final @CookieBehavior int behavior) {
            mCookieBehavior.commit(behavior);
            return this;
        }

        /**
         * Get the assigned cookie lifetime.
         *
         * @return The assigned lifetime, as one of {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
         */
        public @CookieBehavior int getCookieLifetime() {
            return mCookieLifetime.get();
        }

        /**
         * Set the cookie lifetime.
         *
         * @param lifetime The enforced cookie lifetime.
         *                 Use one of the {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
         * @return This Settings instance.
         */
        public @NonNull Settings setCookieLifetime(
                final @CookieLifetime int lifetime) {
            mCookieLifetime.commit(lifetime);
            return this;
        }

    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { NONE, AT_AD, AT_ANALYTIC, AT_SOCIAL, AT_CONTENT,
                      AT_TEST, AT_CRYPTOMINING, AT_FINGERPRINTING,
                      AT_DEFAULT, AT_STRICT,
                      SB_MALWARE, SB_UNWANTED,
                      SB_HARMFUL, SB_PHISHING,
                      CB_DEFAULT, CB_STRICT })
    /* package */ @interface Category {}

    public static final int NONE = 0;

    // Anti-tracking
    /**
     * Block advertisement trackers.
     */
    public static final int AT_AD = 1 << 1;

    /**
     * Block analytics trackers.
     */
    public static final int AT_ANALYTIC = 1 << 2;

    /**
     * Block social trackers.
     */
    public static final int AT_SOCIAL = 1 << 3;

    /**
     * Block content trackers.
     * May cause issues with some web sites.
     */
    public static final int AT_CONTENT = 1 << 4;

    /**
     * Block Gecko test trackers (used for tests).
     */
    public static final int AT_TEST = 1 << 5;

    /**
     * Block cryptocurrency miners.
     */
    public static final int AT_CRYPTOMINING = 1 << 6;

    /**
     * Block fingerprinting trackers.
     */
    public static final int AT_FINGERPRINTING = 1 << 7;

    /**
     * Block ad, analytic, social and test trackers.
     */
    public static final int AT_DEFAULT =
        AT_AD | AT_ANALYTIC | AT_SOCIAL | AT_TEST;

    /**
     * Block all known trackers.
     * May cause issues with some web sites.
     */
    public static final int AT_STRICT =
        AT_DEFAULT | AT_CONTENT | AT_CRYPTOMINING | AT_FINGERPRINTING;

    // Safe browsing
    /**
     * Block malware sites.
     */
    public static final int SB_MALWARE = 1 << 10;

    /**
     * Block unwanted sites.
     */
    public static final int SB_UNWANTED = 1 << 11;

    /**
     * Block harmful sites.
     */
    public static final int SB_HARMFUL = 1 << 12;

    /**
     * Block phishing sites.
     */
    public static final int SB_PHISHING = 1 << 13;

    /**
     * Block all unsafe sites.
     * Blocks all {@link #SB_MALWARE SB_*} types.
     */
    public static final int SB_ALL =
        SB_MALWARE | SB_UNWANTED | SB_HARMFUL | SB_PHISHING;

    /**
     * Recommended content blocking categories.
     * A combination of {@link #AT_DEFAULT} and {@link #SB_ALL}.
     */
    public static final int CB_DEFAULT = SB_ALL | AT_DEFAULT;

    /**
     * Strict content blocking categories.
     * A combination of {@link #AT_STRICT} and {@link #SB_ALL}.
     * May cause issues with some web sites.
     */
    public static final int CB_STRICT = SB_ALL | AT_STRICT;

    // Sync values with nsICookieService.idl.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ COOKIE_ACCEPT_ALL, COOKIE_ACCEPT_FIRST_PARTY,
              COOKIE_ACCEPT_NONE, COOKIE_ACCEPT_VISITED,
              COOKIE_ACCEPT_NON_TRACKERS })
    /* package */ @interface CookieBehavior {}

    /**
     * Accept first-party and third-party cookies and site data.
     */
    public static final int COOKIE_ACCEPT_ALL = 0;

    /**
     * Accept only first-party cookies and site data to block cookies which are
     * not associated with the domain of the visited site.
     */
    public static final int COOKIE_ACCEPT_FIRST_PARTY = 1;

    /**
     * Do not store any cookies and site data.
     */
    public static final int COOKIE_ACCEPT_NONE = 2;

    /**
     * Accept first-party and third-party cookies and site data only from
     * sites previously visited in a first-party context.
     */
    public static final int COOKIE_ACCEPT_VISITED = 3;

    /**
     * Accept only first-party and non-tracking third-party cookies and site data
     * to block cookies which are not associated with the domain of the visited
     * site set by known trackers.
     */
    public static final int COOKIE_ACCEPT_NON_TRACKERS = 4;

    // Sync values with nsICookieService.idl.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ COOKIE_LIFETIME_NORMAL, COOKIE_LIFETIME_RUNTIME,
              COOKIE_LIFETIME_DAYS })
    /* package */ @interface CookieLifetime {}

    /**
     * Accept default cookie lifetime.
     */
    public static final int COOKIE_LIFETIME_NORMAL = 0;

    /**
     * Downgrade cookie lifetime to this runtime's lifetime.
     */
    public static final int COOKIE_LIFETIME_RUNTIME = 2;

    /**
     * Limit cookie lifetime to N days.
     * Defaults to 90 days.
     */
    public static final int COOKIE_LIFETIME_DAYS = 3;

    /**
     * Holds content block event details.
     */
    public static class BlockEvent {
        /**
         * The URI of the blocked resource.
         */
        public final @NonNull String uri;

        /**
         * The category types of the blocked resource.
         * One or more of the {@link #AT_AD AT_* or SB_*} flags.
         */
        public final @Category int categories;

        public BlockEvent(@NonNull final String uri,
                          @Category final int categories) {
            this.uri = uri;
            this.categories = categories;
        }

        /* package */ static BlockEvent fromBundle(
                @NonNull final GeckoBundle bundle) {
            final String uri = bundle.getString("uri");
            final String matchedList = bundle.getString("matchedList");
            final long error = bundle.getLong("error", 0L);

            @Category int cats = NONE;

            if (matchedList != null) {
                cats = ContentBlocking.atListToCat(matchedList) |
                       ContentBlocking.cmListToCat(matchedList) |
                       ContentBlocking.fpListToCat(matchedList);
            } else if (error != 0L) {
                cats = ContentBlocking.errorToCat(error);
            }

            return new BlockEvent(uri, cats);
        }
    }

    /**
     * GeckoSession applications implement this interface to handle content
     * blocking events.
     **/
    public interface Delegate {
        /**
         * A content element has been blocked from loading.
         * Set blocked element categories via {@link GeckoRuntimeSettings} and
         * enable content blocking via {@link GeckoSessionSettings}.
         *
        * @param session The GeckoSession that initiated the callback.
        * @param event The {@link BlockEvent} details.
        */
        @UiThread
        default void onContentBlocked(@NonNull GeckoSession session,
                                      @NonNull BlockEvent event) {}
    }


    private static final String TEST = "test-track-simple";
    private static final String AD = "ads-track-digest256";
    private static final String ANALYTIC = "analytics-track-digest256";
    private static final String SOCIAL = "social-track-digest256";
    private static final String CONTENT = "content-track-digest256";
    private static final String CRYPTOMINING =
        "base-cryptomining-track-digest256";
    private static final String FINGERPRINTING =
        "base-fingerprinting-track-digest256";

    /* package */ static @Category int sbMalwareToCat(final boolean enabled) {
        return enabled ? (SB_MALWARE | SB_UNWANTED | SB_HARMFUL)
                       : NONE;
    }

    /* package */ static @Category int sbPhishingToCat(final boolean enabled) {
        return enabled ? SB_PHISHING
                       : NONE;
    }

    /* package */ static boolean catToSbMalware(@Category final int cat) {
        return (cat & (SB_MALWARE | SB_UNWANTED | SB_HARMFUL)) != 0;
    }

    /* package */ static boolean catToSbPhishing(@Category final int cat) {
        return (cat & SB_PHISHING) != 0;
    }

    /* package */ static String catToAtPref(@Category final int cat) {
        StringBuilder builder = new StringBuilder();

        if ((cat & AT_TEST) != 0) {
            builder.append(TEST).append(',');
        }
        if ((cat & AT_AD) != 0) {
            builder.append(AD).append(',');
        }
        if ((cat & AT_ANALYTIC) != 0) {
            builder.append(ANALYTIC).append(',');
        }
        if ((cat & AT_SOCIAL) != 0) {
            builder.append(SOCIAL).append(',');
        }
        if ((cat & AT_CONTENT) != 0) {
            builder.append(CONTENT).append(',');
        }
        if (builder.length() == 0) {
            return "";
        }
        // Trim final ','.
        return builder.substring(0, builder.length() - 1);
    }

    /* package */ static boolean catToCmPref(@Category final int cat) {
        return (cat & AT_CRYPTOMINING) != 0;
    }

    /* package */ static String catToCmListPref(@Category final int cat) {
        StringBuilder builder = new StringBuilder();

        if ((cat & AT_CRYPTOMINING) != 0) {
            builder.append(CRYPTOMINING);
        }
        return builder.toString();
    }

    /* package */ static boolean catToFpPref(@Category final int cat) {
        return (cat & AT_FINGERPRINTING) != 0;
    }

    /* package */ static String catToFpListPref(@Category final int cat) {
        StringBuilder builder = new StringBuilder();

        if ((cat & AT_FINGERPRINTING) != 0) {
            builder.append(FINGERPRINTING);
        }
        return builder.toString();
    }

    /* package */ static @Category int fpListToCat(final String list) {
        int cat = 0;
        if (list.indexOf(FINGERPRINTING) != -1) {
            cat |= AT_FINGERPRINTING;
        }
        return cat;
    }

    /* package */ static @Category int atListToCat(final String list) {
        int cat = 0;
        if (list.indexOf(TEST) != -1) {
            cat |= AT_TEST;
        }
        if (list.indexOf(AD) != -1) {
            cat |= AT_AD;
        }
        if (list.indexOf(ANALYTIC) != -1) {
            cat |= AT_ANALYTIC;
        }
        if (list.indexOf(SOCIAL) != -1) {
            cat |= AT_SOCIAL;
        }
        if (list.indexOf(CONTENT) != -1) {
            cat |= AT_CONTENT;
        }
        return cat;
    }

    /* package */ static @Category int cmListToCat(final String list) {
        int cat = 0;
        if (list.indexOf(CRYPTOMINING) != -1) {
            cat |= AT_CRYPTOMINING;
        }
        return cat;
    }

    /* package */ static @Category int errorToCat(final long error) {
        // Match flags with XPCOM ErrorList.h.
        if (error == 0x805D001FL) {
            return SB_PHISHING;
        }
        if (error == 0x805D001EL) {
            return SB_MALWARE;
        }
        if (error == 0x805D0023L) {
            return SB_UNWANTED;
        }
        if (error == 0x805D0026L) {
            return SB_HARMFUL;
        }
        return NONE;
    }
}
