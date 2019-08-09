/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import android.os.Parcelable;
import android.os.Parcel;
import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.text.TextUtils;

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
             * Set anti-tracking categories.
             *
             * @param cat The categories of resources that should be blocked.
             *            Use one or more of the
             *            {@link ContentBlocking.AntiTracking} flags.
             * @return This Builder instance.
             */
            public @NonNull Builder antiTracking(final @CBAntiTracking int cat) {
                getSettings().setAntiTracking(cat);
                return this;
            }

            /**
             * Set safe browsing categories.
             *
             * @param cat The categories of resources that should be blocked.
             *            Use one or more of the
             *            {@link ContentBlocking.SafeBrowsing} flags.
             * @return This Builder instance.
             */
            public @NonNull Builder safeBrowsing(final @CBSafeBrowsing int cat) {
                getSettings().setSafeBrowsing(cat);
                return this;
            }

            /**
             * Set cookie storage behavior.
             *
             * @param behavior The storage behavior that should be applied.
             *                 Use one of the {@link CookieBehavior} flags.
             * @return The Builder instance.
             */
            public @NonNull Builder cookieBehavior(final @CBCookieBehavior int behavior) {
                getSettings().setCookieBehavior(behavior);
                return this;
            }

            /**
             * Set the cookie lifetime.
             *
             * @param lifetime The enforced cookie lifetime.
             *                 Use one of the {@link CookieLifetime} flags.
             * @return The Builder instance.
             */
            public @NonNull Builder cookieLifetime(final @CBCookieLifetime int lifetime) {
                getSettings().setCookieLifetime(lifetime);
                return this;
            }
        }

        /* package */ final Pref<String> mAt = new Pref<String>(
            "urlclassifier.trackingTable",
            ContentBlocking.catToAtPref(AntiTracking.DEFAULT));
        /* package */ final Pref<Boolean> mCm = new Pref<Boolean>(
            "privacy.trackingprotection.cryptomining.enabled", false);
        /* package */ final Pref<String> mCmList = new Pref<String>(
            "urlclassifier.features.cryptomining.blacklistTables",
            ContentBlocking.catToCmListPref(AntiTracking.NONE));
        /* package */ final Pref<Boolean> mFp = new Pref<Boolean>(
            "privacy.trackingprotection.fingerprinting.enabled", false);
        /* package */ final Pref<String> mFpList = new Pref<String>(
            "urlclassifier.features.fingerprinting.blacklistTables",
            ContentBlocking.catToFpListPref(AntiTracking.NONE));

        /* package */ final Pref<Boolean> mSbMalware = new Pref<Boolean>(
            "browser.safebrowsing.malware.enabled", true);
        /* package */ final Pref<Boolean> mSbPhishing = new Pref<Boolean>(
            "browser.safebrowsing.phishing.enabled", true);
        /* package */ final Pref<Integer> mCookieBehavior = new Pref<Integer>(
            "network.cookie.cookieBehavior", CookieBehavior.ACCEPT_NON_TRACKERS);
        /* package */ final Pref<Integer> mCookieLifetime = new Pref<Integer>(
            "network.cookie.lifetimePolicy", CookieLifetime.NORMAL);

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
         * Set anti-tracking categories.
         *
         * @param cat The categories of resources that should be blocked.
         *            Use one or more of the
         *            {@link ContentBlocking.AntiTracking} flags.
         * @return This Settings instance.
         */
        public @NonNull Settings setAntiTracking(final @CBAntiTracking int cat) {
            mAt.commit(ContentBlocking.catToAtPref(cat));

            mCm.commit(ContentBlocking.catToCmPref(cat));
            mCmList.commit(ContentBlocking.catToCmListPref(cat));

            mFp.commit(ContentBlocking.catToFpPref(cat));
            mFpList.commit(ContentBlocking.catToFpListPref(cat));
            return this;
        }

        /**
         * Set safe browsing categories.
         *
         * @param cat The categories of resources that should be blocked.
         *            Use one or more of the
         *            {@link ContentBlocking.SafeBrowsing} flags.
         * @return This Settings instance.
         */
        public @NonNull Settings setSafeBrowsing(final @CBSafeBrowsing int cat) {
            mSbMalware.commit(ContentBlocking.catToSbMalware(cat));
            mSbPhishing.commit(ContentBlocking.catToSbPhishing(cat));
            return this;
        }

        /**
         * Get the set anti-tracking categories.
         *
         * @return The categories of resources to be blocked.
         */
        public @CBAntiTracking int getAntiTrackingCategories() {
            return ContentBlocking.atListToAtCat(mAt.get()) |
                   ContentBlocking.cmListToAtCat(mCmList.get()) |
                   ContentBlocking.fpListToAtCat(mFpList.get());
        }

        /**
         * Get the set safe browsing categories.
         *
         * @return The categories of resources to be blocked.
         */
        public @CBAntiTracking int getSafeBrowsingCategories() {
            return ContentBlocking.sbMalwareToSbCat(mSbMalware.get()) |
                   ContentBlocking.sbPhishingToSbCat(mSbPhishing.get());
        }

        /**
         * Get the assigned cookie storage behavior.
         *
         * @return The assigned behavior, as one of {@link CookieBehavior} flags.
         */
        public @CBCookieBehavior int getCookieBehavior() {
            return mCookieBehavior.get();
        }

        /**
         * Set cookie storage behavior.
         *
         * @param behavior The storage behavior that should be applied.
         *                 Use one of the {@link CookieBehavior} flags.
         * @return This Settings instance.
         */
        public @NonNull Settings setCookieBehavior(
                final @CBCookieBehavior int behavior) {
            mCookieBehavior.commit(behavior);
            return this;
        }

        /**
         * Get the assigned cookie lifetime.
         *
         * @return The assigned lifetime, as one of {@link CookieLifetime} flags.
         */
        public @CBCookieLifetime int getCookieLifetime() {
            return mCookieLifetime.get();
        }

        /**
         * Set the cookie lifetime.
         *
         * @param lifetime The enforced cookie lifetime.
         *                 Use one of the {@link CookieLifetime} flags.
         * @return This Settings instance.
         */
        public @NonNull Settings setCookieLifetime(
                final @CBCookieLifetime int lifetime) {
            mCookieLifetime.commit(lifetime);
            return this;
        }

        public static final Parcelable.Creator<Settings> CREATOR
                = new Parcelable.Creator<Settings>() {
                    @Override
                    public Settings createFromParcel(final Parcel in) {
                        final Settings settings = new Settings();
                        settings.readFromParcel(in);
                        return settings;
                    }

                    @Override
                    public Settings[] newArray(final int size) {
                        return new Settings[size];
                    }
                };
    }

    public static class AntiTracking {
        public static final int NONE = 0;

        /**
         * Block advertisement trackers.
         */
        public static final int AD = 1 << 1;

        /**
         * Block analytics trackers.
         */
        public static final int ANALYTIC = 1 << 2;

        /**
         * Block social trackers.
         */
        public static final int SOCIAL = 1 << 3;

        /**
         * Block content trackers.
         * May cause issues with some web sites.
         */
        public static final int CONTENT = 1 << 4;

        /**
         * Block Gecko test trackers (used for tests).
         */
        public static final int TEST = 1 << 5;

        /**
         * Block cryptocurrency miners.
         */
        public static final int CRYPTOMINING = 1 << 6;

        /**
         * Block fingerprinting trackers.
         */
        public static final int FINGERPRINTING = 1 << 7;

        /**
         * Block ad, analytic, social and test trackers.
         */
        public static final int DEFAULT = AD | ANALYTIC | SOCIAL | TEST;

        /**
         * Block all known trackers.
         * May cause issues with some web sites.
         */
        public static final int STRICT = DEFAULT | CONTENT | CRYPTOMINING | FINGERPRINTING;

        protected AntiTracking() {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { AntiTracking.AD, AntiTracking.ANALYTIC,
                      AntiTracking.SOCIAL, AntiTracking.CONTENT,
                      AntiTracking.TEST, AntiTracking.CRYPTOMINING,
                      AntiTracking.FINGERPRINTING,
                      AntiTracking.DEFAULT, AntiTracking.STRICT,
                      AntiTracking.NONE })
    /* package */ @interface CBAntiTracking {}

    public static class SafeBrowsing {
        public static final int NONE = 0;

        /**
         * Block malware sites.
         */
        public static final int MALWARE = 1 << 10;

        /**
         * Block unwanted sites.
         */
        public static final int UNWANTED = 1 << 11;

        /**
         * Block harmful sites.
         */
        public static final int HARMFUL = 1 << 12;

        /**
         * Block phishing sites.
         */
        public static final int PHISHING = 1 << 13;

        /**
         * Block all unsafe sites.
         */
        public static final int DEFAULT = MALWARE | UNWANTED | HARMFUL | PHISHING;

        protected SafeBrowsing() {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { SafeBrowsing.MALWARE, SafeBrowsing.UNWANTED,
                      SafeBrowsing.HARMFUL, SafeBrowsing.PHISHING,
                      SafeBrowsing.DEFAULT, SafeBrowsing.NONE })
    /* package */ @interface CBSafeBrowsing {}


    // Sync values with nsICookieService.idl.
    public static class CookieBehavior {
        /**
         * Accept first-party and third-party cookies and site data.
         */
        public static final int ACCEPT_ALL = 0;

        /**
         * Accept only first-party cookies and site data to block cookies which are
         * not associated with the domain of the visited site.
         */
        public static final int ACCEPT_FIRST_PARTY = 1;

        /**
         * Do not store any cookies and site data.
         */
        public static final int ACCEPT_NONE = 2;

        /**
         * Accept first-party and third-party cookies and site data only from
         * sites previously visited in a first-party context.
         */
        public static final int ACCEPT_VISITED = 3;

        /**
         * Accept only first-party and non-tracking third-party cookies and site data
         * to block cookies which are not associated with the domain of the visited
         * site set by known trackers.
         */
        public static final int ACCEPT_NON_TRACKERS = 4;

        protected CookieBehavior() {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ CookieBehavior.ACCEPT_ALL, CookieBehavior.ACCEPT_FIRST_PARTY,
              CookieBehavior.ACCEPT_NONE, CookieBehavior.ACCEPT_VISITED,
              CookieBehavior.ACCEPT_NON_TRACKERS })
    /* package */ @interface CBCookieBehavior {}

    // Sync values with nsICookieService.idl.
    public static class CookieLifetime {
        /**
         * Accept default cookie lifetime.
         */
        public static final int NORMAL = 0;

        /**
         * Downgrade cookie lifetime to this runtime's lifetime.
         */
        public static final int RUNTIME = 2;

        /**
         * Limit cookie lifetime to N days.
         * Defaults to 90 days.
         */
        public static final int DAYS = 3;

        protected CookieLifetime() {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ CookieLifetime.NORMAL, CookieLifetime.RUNTIME,
              CookieLifetime.DAYS })
    /* package */ @interface CBCookieLifetime {}

    /**
     * Holds content block event details.
     */
    public static class BlockEvent {
        /**
         * The URI of the blocked resource.
         */
        public final @NonNull String uri;

        private final @CBAntiTracking int mAntiTrackingCat;
        private final @CBSafeBrowsing int mSafeBrowsingCat;
        private final @CBCookieBehavior int mCookieBehaviorCat;
        private final boolean mIsBlocking;

        public BlockEvent(@NonNull final String uri,
                          final @CBAntiTracking int atCat,
                          final @CBSafeBrowsing int sbCat,
                          final @CBCookieBehavior int cbCat,
                          final boolean isBlocking) {
            this.uri = uri;
            this.mAntiTrackingCat = atCat;
            this.mSafeBrowsingCat = sbCat;
            this.mCookieBehaviorCat = cbCat;
            this.mIsBlocking = isBlocking;
        }

        /**
         * The anti-tracking category types of the blocked resource.
         * One or more of the {@link AntiTracking} flags.
         */
        @UiThread
        public @CBAntiTracking int getAntiTrackingCategory() {
            return mAntiTrackingCat;
        }

        /**
         * The safe browsing category types of the blocked resource.
         * One or more of the {@link AntiTracking} flags.
         */
        @UiThread
        public @CBSafeBrowsing int getSafeBrowsingCategory() {
            return mSafeBrowsingCat;
        }

        /**
         * The cookie types of the blocked resource.
         * One or more of the {@link CookieBehavior} flags.
         */
        @UiThread
        public @CBCookieBehavior int getCookieBehaviorCategory() {
            return mCookieBehaviorCat;
        }


        /* package */ static BlockEvent fromBundle(
                @NonNull final GeckoBundle bundle) {
            final String uri = bundle.getString("uri");
            final String blockedList = bundle.getString("blockedList");
            final String loadedList =
                TextUtils.join(",", bundle.getStringArray("loadedLists"));
            final long error = bundle.getLong("error", 0L);
            final long category = bundle.getLong("category", 0L);

            final String matchedList =
                blockedList != null ? blockedList : loadedList;

            final boolean blocking =
                loadedList.isEmpty() &&
                (blockedList != null ||
                 error != 0L ||
                 ContentBlocking.isBlockingGeckoCbCat(category));

            return new BlockEvent(
                uri,
                ContentBlocking.atListToAtCat(matchedList) |
                    ContentBlocking.cmListToAtCat(matchedList) |
                    ContentBlocking.fpListToAtCat(matchedList),
                ContentBlocking.errorToSbCat(error),
                ContentBlocking.geckoCatToCbCat(category),
                blocking);
        }

        @UiThread
        public boolean isBlocking() {
            return mIsBlocking;
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

        /**
         * A content element that could be blocked has been loaded.
         *
        * @param session The GeckoSession that initiated the callback.
        * @param event The {@link BlockEvent} details.
        */
        @UiThread
        default void onContentLoaded(@NonNull GeckoSession session,
                                     @NonNull BlockEvent event) {}
    }

    private static final String TEST = "moztest-track-simple";
    private static final String AD = "ads-track-digest256";
    private static final String ANALYTIC = "analytics-track-digest256";
    private static final String SOCIAL = "social-track-digest256";
    private static final String CONTENT = "content-track-digest256";
    private static final String CRYPTOMINING =
        "base-cryptomining-track-digest256";
    private static final String FINGERPRINTING =
        "base-fingerprinting-track-digest256";

    /* package */ static @CBSafeBrowsing int sbMalwareToSbCat(final boolean enabled) {
        return enabled ? (SafeBrowsing.MALWARE | SafeBrowsing.UNWANTED | SafeBrowsing.HARMFUL)
                       : SafeBrowsing.NONE;
    }

    /* package */ static @CBSafeBrowsing int sbPhishingToSbCat(final boolean enabled) {
        return enabled ? SafeBrowsing.PHISHING
                       : SafeBrowsing.NONE;
    }

    /* package */ static boolean catToSbMalware(@CBAntiTracking final int cat) {
        return (cat & (SafeBrowsing.MALWARE | SafeBrowsing.UNWANTED | SafeBrowsing.HARMFUL)) != 0;
    }

    /* package */ static boolean catToSbPhishing(@CBAntiTracking final int cat) {
        return (cat & SafeBrowsing.PHISHING) != 0;
    }

    /* package */ static String catToAtPref(@CBAntiTracking final int cat) {
        StringBuilder builder = new StringBuilder();

        if ((cat & AntiTracking.TEST) != 0) {
            builder.append(TEST).append(',');
        }
        if ((cat & AntiTracking.AD) != 0) {
            builder.append(AD).append(',');
        }
        if ((cat & AntiTracking.ANALYTIC) != 0) {
            builder.append(ANALYTIC).append(',');
        }
        if ((cat & AntiTracking.SOCIAL) != 0) {
            builder.append(SOCIAL).append(',');
        }
        if ((cat & AntiTracking.CONTENT) != 0) {
            builder.append(CONTENT).append(',');
        }
        if (builder.length() == 0) {
            return "";
        }
        // Trim final ','.
        return builder.substring(0, builder.length() - 1);
    }

    /* package */ static boolean catToCmPref(@CBAntiTracking final int cat) {
        return (cat & AntiTracking.CRYPTOMINING) != 0;
    }

    /* package */ static String catToCmListPref(@CBAntiTracking final int cat) {
        StringBuilder builder = new StringBuilder();

        if ((cat & AntiTracking.CRYPTOMINING) != 0) {
            builder.append(CRYPTOMINING);
        }
        return builder.toString();
    }

    /* package */ static boolean catToFpPref(@CBAntiTracking final int cat) {
        return (cat & AntiTracking.FINGERPRINTING) != 0;
    }

    /* package */ static String catToFpListPref(@CBAntiTracking final int cat) {
        StringBuilder builder = new StringBuilder();

        if ((cat & AntiTracking.FINGERPRINTING) != 0) {
            builder.append(FINGERPRINTING);
        }
        return builder.toString();
    }

    /* package */ static @CBAntiTracking int fpListToAtCat(final String list) {
        int cat = AntiTracking.NONE;
        if (list == null) {
            return cat;
        }
        if (list.indexOf(FINGERPRINTING) != -1) {
            cat |= AntiTracking.FINGERPRINTING;
        }
        return cat;
    }

    /* package */ static @CBAntiTracking int atListToAtCat(final String list) {
        int cat = AntiTracking.NONE;

        if (list == null) {
            return cat;
        }
        if (list.indexOf(TEST) != -1) {
            cat |= AntiTracking.TEST;
        }
        if (list.indexOf(AD) != -1) {
            cat |= AntiTracking.AD;
        }
        if (list.indexOf(ANALYTIC) != -1) {
            cat |= AntiTracking.ANALYTIC;
        }
        if (list.indexOf(SOCIAL) != -1) {
            cat |= AntiTracking.SOCIAL;
        }
        if (list.indexOf(CONTENT) != -1) {
            cat |= AntiTracking.CONTENT;
        }
        return cat;
    }

    /* package */ static @CBAntiTracking int cmListToAtCat(final String list) {
        int cat = AntiTracking.NONE;
        if (list == null) {
            return cat;
        }
        if (list.indexOf(CRYPTOMINING) != -1) {
            cat |= AntiTracking.CRYPTOMINING;
        }
        return cat;
    }

    /* package */ static @CBSafeBrowsing int errorToSbCat(final long error) {
        // Match flags with XPCOM ErrorList.h.
        if (error == 0x805D001FL) {
            return SafeBrowsing.PHISHING;
        }
        if (error == 0x805D001EL) {
            return SafeBrowsing.MALWARE;
        }
        if (error == 0x805D0023L) {
            return SafeBrowsing.UNWANTED;
        }
        if (error == 0x805D0026L) {
            return SafeBrowsing.HARMFUL;
        }
        return SafeBrowsing.NONE;
    }

    // Match flags with nsIWebProgressListener.idl.
    private static final long STATE_COOKIES_LOADED = 0x8000L;
    private static final long STATE_COOKIES_BLOCKED_TRACKER = 0x20000000L;
    private static final long STATE_COOKIES_BLOCKED_ALL = 0x40000000L;
    private static final long STATE_COOKIES_BLOCKED_FOREIGN = 0x80L;

    /* package */ static boolean isBlockingGeckoCbCat(final long geckoCat) {
        return
            (geckoCat &
                (STATE_COOKIES_BLOCKED_TRACKER |
                 STATE_COOKIES_BLOCKED_ALL |
                 STATE_COOKIES_BLOCKED_FOREIGN))
            != 0;
    }

    /* package */ static @CBCookieBehavior int geckoCatToCbCat(
            final long geckoCat) {
        if ((geckoCat & STATE_COOKIES_LOADED) != 0) {
            // We don't know which setting would actually block this cookie, so
            // we return the most strict value.
            return CookieBehavior.ACCEPT_NONE;
        }
        if ((geckoCat & STATE_COOKIES_BLOCKED_FOREIGN) != 0) {
            return CookieBehavior.ACCEPT_FIRST_PARTY;
        }
        if ((geckoCat & STATE_COOKIES_BLOCKED_TRACKER) != 0) {
            return CookieBehavior.ACCEPT_NON_TRACKERS;
        }
        if ((geckoCat & STATE_COOKIES_BLOCKED_ALL) != 0) {
            return CookieBehavior.ACCEPT_NONE;
        }
        // TODO: There are more reasons why cookies may be blocked.
        return CookieBehavior.ACCEPT_ALL;
    }
}
