/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.annotation.SuppressLint;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import org.mozilla.gecko.util.GeckoBundle;

/** Content Blocking API to hold and control anti-tracking, cookie and Safe Browsing settings. */
@AnyThread
public class ContentBlocking {
  /** {@link SafeBrowsingProvider} configuration for Google's legacy SafeBrowsing server. */
  public static final SafeBrowsingProvider GOOGLE_LEGACY_SAFE_BROWSING_PROVIDER =
      SafeBrowsingProvider.withName("google")
          .version("2.2")
          .lists(
              "goog-badbinurl-shavar",
              "goog-downloadwhite-digest256",
              "goog-phish-shavar",
              "googpub-phish-shavar",
              "goog-malware-shavar",
              "goog-unwanted-shavar")
          .updateUrl(
              "https://safebrowsing.google.com/safebrowsing/downloads?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2&key=%GOOGLE_SAFEBROWSING_API_KEY%")
          .getHashUrl(
              "https://safebrowsing.google.com/safebrowsing/gethash?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2")
          .reportUrl("https://safebrowsing.google.com/safebrowsing/diagnostic?site=")
          .reportPhishingMistakeUrl("https://%LOCALE%.phish-error.mozilla.com/?url=")
          .reportMalwareMistakeUrl("https://%LOCALE%.malware-error.mozilla.com/?url=")
          .advisoryUrl("https://developers.google.com/safe-browsing/v4/advisory")
          .advisoryName("Google Safe Browsing")
          .build();

  /** {@link SafeBrowsingProvider} configuration for Google's SafeBrowsing server. */
  public static final SafeBrowsingProvider GOOGLE_SAFE_BROWSING_PROVIDER =
      SafeBrowsingProvider.withName("google4")
          .version("4")
          .lists(
              "goog-badbinurl-proto",
              "goog-downloadwhite-proto",
              "goog-phish-proto",
              "googpub-phish-proto",
              "goog-malware-proto",
              "goog-unwanted-proto",
              "goog-harmful-proto",
              "goog-passwordwhite-proto")
          .updateUrl(
              "https://safebrowsing.googleapis.com/v4/threatListUpdates:fetch?$ct=application/x-protobuf&key=%GOOGLE_SAFEBROWSING_API_KEY%&$httpMethod=POST")
          .getHashUrl(
              "https://safebrowsing.googleapis.com/v4/fullHashes:find?$ct=application/x-protobuf&key=%GOOGLE_SAFEBROWSING_API_KEY%&$httpMethod=POST")
          .reportUrl("https://safebrowsing.google.com/safebrowsing/diagnostic?site=")
          .reportPhishingMistakeUrl("https://%LOCALE%.phish-error.mozilla.com/?url=")
          .reportMalwareMistakeUrl("https://%LOCALE%.malware-error.mozilla.com/?url=")
          .advisoryUrl("https://developers.google.com/safe-browsing/v4/advisory")
          .advisoryName("Google Safe Browsing")
          .dataSharingUrl(
              "https://safebrowsing.googleapis.com/v4/threatHits?$ct=application/x-protobuf&key=%GOOGLE_SAFEBROWSING_API_KEY%&$httpMethod=POST")
          .dataSharingEnabled(false)
          .build();

  // This class shouldn't be instantiated
  protected ContentBlocking() {}

  @AnyThread
  public static class Settings extends RuntimeSettings {
    private final Map<String, SafeBrowsingProvider> mSafeBrowsingProviders = new HashMap<>();

    private static final SafeBrowsingProvider[] DEFAULT_PROVIDERS = {
      ContentBlocking.GOOGLE_LEGACY_SAFE_BROWSING_PROVIDER,
      ContentBlocking.GOOGLE_SAFE_BROWSING_PROVIDER
    };

    @AnyThread
    public static class Builder extends RuntimeSettings.Builder<Settings> {
      @Override
      protected @NonNull Settings newSettings(final @Nullable Settings settings) {
        return new Settings(settings);
      }

      /**
       * Set custom safe browsing providers.
       *
       * @param providers one or more custom providers.
       * @return This Builder instance.
       * @see SafeBrowsingProvider
       */
      public @NonNull Builder safeBrowsingProviders(
          final @NonNull SafeBrowsingProvider... providers) {
        getSettings().setSafeBrowsingProviders(providers);
        return this;
      }

      /**
       * Set the safe browsing table for phishing threats.
       *
       * @param safeBrowsingPhishingTable one or more lists for safe browsing phishing.
       * @return This Builder instance.
       * @see SafeBrowsingProvider
       */
      public @NonNull Builder safeBrowsingPhishingTable(
          final @NonNull String[] safeBrowsingPhishingTable) {
        getSettings().setSafeBrowsingPhishingTable(safeBrowsingPhishingTable);
        return this;
      }

      /**
       * Set the safe browsing table for malware threats.
       *
       * @param safeBrowsingMalwareTable one or more lists for safe browsing malware.
       * @return This Builder instance.
       * @see SafeBrowsingProvider
       */
      public @NonNull Builder safeBrowsingMalwareTable(
          final @NonNull String[] safeBrowsingMalwareTable) {
        getSettings().setSafeBrowsingMalwareTable(safeBrowsingMalwareTable);
        return this;
      }

      /**
       * Set anti-tracking categories.
       *
       * @param cat The categories of resources that should be blocked. Use one or more of the
       *     {@link ContentBlocking.AntiTracking} flags.
       * @return This Builder instance.
       */
      public @NonNull Builder antiTracking(final @CBAntiTracking int cat) {
        getSettings().setAntiTracking(cat);
        return this;
      }

      /**
       * Set safe browsing categories.
       *
       * @param cat The categories of resources that should be blocked. Use one or more of the
       *     {@link ContentBlocking.SafeBrowsing} flags.
       * @return This Builder instance.
       */
      public @NonNull Builder safeBrowsing(final @CBSafeBrowsing int cat) {
        getSettings().setSafeBrowsing(cat);
        return this;
      }

      /**
       * Set cookie storage behavior.
       *
       * @param behavior The storage behavior that should be applied. Use one of the {@link
       *     CookieBehavior} flags.
       * @return The Builder instance.
       */
      public @NonNull Builder cookieBehavior(final @CBCookieBehavior int behavior) {
        getSettings().setCookieBehavior(behavior);
        return this;
      }

      /**
       * Set cookie storage behavior in private browsing mode.
       *
       * @param behavior The storage behavior that should be applied. Use one of the {@link
       *     CookieBehavior} flags.
       * @return The Builder instance.
       */
      public @NonNull Builder cookieBehaviorPrivateMode(final @CBCookieBehavior int behavior) {
        getSettings().setCookieBehaviorPrivateMode(behavior);
        return this;
      }

      /**
       * Set the cookie lifetime.
       *
       * @param lifetime The enforced cookie lifetime. Use one of the {@link CookieLifetime} flags.
       * @return The Builder instance.
       * @deprecated This feature is not supported anymore.
       */
      @Deprecated
      @DeprecationSchedule(id = "cookie-lifetime-policy", version = 106)
      public @NonNull Builder cookieLifetime(final @CBCookieLifetime int lifetime) {
        return this;
      }

      /**
       * Set the ETP behavior level.
       *
       * @param level The level of ETP blocking to use. Only takes effect if cookie behavior is set
       *     to {@link ContentBlocking.CookieBehavior#ACCEPT_NON_TRACKERS} or {@link
       *     ContentBlocking.CookieBehavior#ACCEPT_FIRST_PARTY_AND_ISOLATE_OTHERS}.
       * @return The Builder instance.
       */
      public @NonNull Builder enhancedTrackingProtectionLevel(final @CBEtpLevel int level) {
        getSettings().setEnhancedTrackingProtectionLevel(level);
        return this;
      }

      /**
       * Set whether or not strict social tracking protection is enabled. This will block resources
       * from loading if they are on the social tracking protection list, rather than just blocking
       * cookies as with normal social tracking protection.
       *
       * @param enabled A boolean indicating whether or not strict social tracking protection should
       *     be enabled.
       * @return The builder instance.
       */
      public @NonNull Builder strictSocialTrackingProtection(final boolean enabled) {
        getSettings().setStrictSocialTrackingProtection(enabled);
        return this;
      }

      /**
       * Set whether or not to automatically purge tracking cookies. This will purge cookies from
       * tracking sites that do not have recent user interaction provided that the cookie behavior
       * is set to either {@link ContentBlocking.CookieBehavior#ACCEPT_NON_TRACKERS} or {@link
       * ContentBlocking.CookieBehavior#ACCEPT_FIRST_PARTY_AND_ISOLATE_OTHERS}.
       *
       * @param enabled A boolean indicating whether or not cookie purging should be enabled.
       * @return The builder instance.
       */
      public @NonNull Builder cookiePurging(final boolean enabled) {
        getSettings().setCookiePurging(enabled);
        return this;
      }
    }

    /* package */ final Pref<String> mAt =
        new Pref<String>(
            "urlclassifier.trackingTable", ContentBlocking.catToAtPref(AntiTracking.DEFAULT));
    /* package */ final Pref<Boolean> mCm =
        new Pref<Boolean>("privacy.trackingprotection.cryptomining.enabled", false);
    /* package */ final Pref<String> mCmList =
        new Pref<String>(
            "urlclassifier.features.cryptomining.blacklistTables",
            ContentBlocking.catToCmListPref(AntiTracking.NONE));
    /* package */ final Pref<Boolean> mFp =
        new Pref<Boolean>("privacy.trackingprotection.fingerprinting.enabled", false);
    /* package */ final Pref<String> mFpList =
        new Pref<String>(
            "urlclassifier.features.fingerprinting.blacklistTables",
            ContentBlocking.catToFpListPref(AntiTracking.NONE));
    /* package */ final Pref<Boolean> mSt =
        new Pref<Boolean>("privacy.socialtracking.block_cookies.enabled", false);
    /* package */ final Pref<Boolean> mStStrict =
        new Pref<Boolean>("privacy.trackingprotection.socialtracking.enabled", false);
    /* package */ final Pref<String> mStList =
        new Pref<String>(
            "urlclassifier.features.socialtracking.annotate.blacklistTables",
            ContentBlocking.catToStListPref(AntiTracking.NONE));

    /* package */ final Pref<Boolean> mSbMalware =
        new Pref<Boolean>("browser.safebrowsing.malware.enabled", true);
    /* package */ final Pref<Boolean> mSbPhishing =
        new Pref<Boolean>("browser.safebrowsing.phishing.enabled", true);
    /* package */ final Pref<Integer> mCookieBehavior =
        new Pref<Integer>("network.cookie.cookieBehavior", CookieBehavior.ACCEPT_NON_TRACKERS);
    /* package */ final Pref<Integer> mCookieBehaviorPrivateMode =
        new Pref<Integer>(
            "network.cookie.cookieBehavior.pbmode", CookieBehavior.ACCEPT_NON_TRACKERS);
    /* package */ final Pref<Boolean> mCookiePurging =
        new Pref<Boolean>("privacy.purge_trackers.enabled", false);

    /* package */ final Pref<Boolean> mEtpEnabled =
        new Pref<Boolean>("privacy.trackingprotection.annotate_channels", false);
    /* package */ final Pref<Boolean> mEtpStrict =
        new Pref<Boolean>("privacy.annotate_channels.strict_list.enabled", false);

    /* package */ final Pref<String> mSafeBrowsingMalwareTable =
        new Pref<>(
            "urlclassifier.malwareTable",
            ContentBlocking.listsToPref(
                "goog-malware-proto",
                "goog-unwanted-proto",
                "moztest-harmful-simple",
                "moztest-malware-simple",
                "moztest-unwanted-simple"));
    /* package */ final Pref<String> mSafeBrowsingPhishingTable =
        new Pref<>(
            "urlclassifier.phishTable",
            ContentBlocking.listsToPref(
                // In official builds, we are allowed to use Google's private phishing
                // list (see bug 1288840).
                BuildConfig.MOZILLA_OFFICIAL ? "goog-phish-proto" : "googpub-phish-proto",
                "moztest-phish-simple"));

    /** Construct default settings. */
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
    /* package */ Settings(
        final @Nullable RuntimeSettings parent, final @Nullable Settings settings) {
      super(parent);

      if (settings != null) {
        updatePrefs(settings);
      } else {
        // Set default browsing providers
        setSafeBrowsingProviders(DEFAULT_PROVIDERS);
      }
    }

    @Override
    protected void updatePrefs(final @NonNull RuntimeSettings settings) {
      super.updatePrefs(settings);

      final ContentBlocking.Settings source = (ContentBlocking.Settings) settings;
      for (final SafeBrowsingProvider provider : source.mSafeBrowsingProviders.values()) {
        mSafeBrowsingProviders.put(provider.getName(), new SafeBrowsingProvider(this, provider));
      }
    }

    /**
     * Get the collection of {@link SafeBrowsingProvider} for this runtime.
     *
     * @return an unmodifiable collection of {@link SafeBrowsingProvider}
     * @see SafeBrowsingProvider
     */
    public @NonNull Collection<SafeBrowsingProvider> getSafeBrowsingProviders() {
      return Collections.unmodifiableCollection(mSafeBrowsingProviders.values());
    }

    /**
     * Sets the collection of {@link SafeBrowsingProvider} for this runtime.
     *
     * <p>By default the collection is composed of {@link
     * ContentBlocking#GOOGLE_LEGACY_SAFE_BROWSING_PROVIDER} and {@link
     * ContentBlocking#GOOGLE_SAFE_BROWSING_PROVIDER}.
     *
     * @param providers {@link SafeBrowsingProvider} instances for this runtime.
     * @return the {@link Settings} instance.
     * @see SafeBrowsingProvider
     */
    public @NonNull Settings setSafeBrowsingProviders(
        final @NonNull SafeBrowsingProvider... providers) {
      mSafeBrowsingProviders.clear();

      for (final SafeBrowsingProvider provider : providers) {
        mSafeBrowsingProviders.put(provider.getName(), new SafeBrowsingProvider(this, provider));
      }

      return this;
    }

    /**
     * Get the table for SafeBrowsing Phishing. The identifiers present in this table must match one
     * of the identifiers present in {@link SafeBrowsingProvider#getLists}.
     *
     * @return an array of identifiers for SafeBrowsing's Phishing feature
     * @see SafeBrowsingProvider.Builder#lists
     */
    public @NonNull String[] getSafeBrowsingPhishingTable() {
      return ContentBlocking.prefToLists(mSafeBrowsingPhishingTable.get());
    }

    /**
     * Sets the table for SafeBrowsing Phishing.
     *
     * @param table an array of identifiers for SafeBrowsing's Phishing feature.
     * @return this {@link Settings} instance.
     * @see SafeBrowsingProvider.Builder#lists
     */
    public @NonNull Settings setSafeBrowsingPhishingTable(final @NonNull String... table) {
      mSafeBrowsingPhishingTable.commit(ContentBlocking.listsToPref(table));
      return this;
    }

    /**
     * Get the table for SafeBrowsing Malware. The identifiers present in this table must match one
     * of the identifiers present in {@link SafeBrowsingProvider#getLists}.
     *
     * @return an array of identifiers for SafeBrowsing's Malware feature
     * @see SafeBrowsingProvider.Builder#lists
     */
    public @NonNull String[] getSafeBrowsingMalwareTable() {
      return ContentBlocking.prefToLists(mSafeBrowsingMalwareTable.get());
    }

    /**
     * Sets the table for SafeBrowsing Malware.
     *
     * @param table an array of identifiers for SafeBrowsing's Malware feature.
     * @return this {@link Settings} instance.
     * @see SafeBrowsingProvider.Builder#lists
     */
    public @NonNull Settings setSafeBrowsingMalwareTable(final @NonNull String... table) {
      mSafeBrowsingMalwareTable.commit(ContentBlocking.listsToPref(table));
      return this;
    }

    /**
     * Set anti-tracking categories.
     *
     * @param cat The categories of resources that should be blocked. Use one or more of the {@link
     *     ContentBlocking.AntiTracking} flags.
     * @return This Settings instance.
     */
    public @NonNull Settings setAntiTracking(final @CBAntiTracking int cat) {
      mAt.commit(ContentBlocking.catToAtPref(cat));

      mCm.commit(ContentBlocking.catToCmPref(cat));
      mCmList.commit(ContentBlocking.catToCmListPref(cat));

      mFp.commit(ContentBlocking.catToFpPref(cat));
      mFpList.commit(ContentBlocking.catToFpListPref(cat));

      mSt.commit(ContentBlocking.catToStPref(cat));
      mStList.commit(ContentBlocking.catToStListPref(cat));
      return this;
    }

    /**
     * Set the ETP behavior level.
     *
     * @param level The level of ETP blocking to use; must be one of {@link
     *     ContentBlocking.EtpLevel} flags. Only takes effect if the cookie behavior is {@link
     *     ContentBlocking.CookieBehavior#ACCEPT_NON_TRACKERS} or {@link
     *     ContentBlocking.CookieBehavior#ACCEPT_FIRST_PARTY_AND_ISOLATE_OTHERS}.
     * @return This Settings instance.
     */
    public @NonNull Settings setEnhancedTrackingProtectionLevel(final @CBEtpLevel int level) {
      mEtpEnabled.commit(
          level == ContentBlocking.EtpLevel.DEFAULT || level == ContentBlocking.EtpLevel.STRICT);
      mEtpStrict.commit(level == ContentBlocking.EtpLevel.STRICT);
      return this;
    }

    /**
     * Set whether or not strict social tracking protection is enabled (ie, whether to block content
     * or just cookies). Will only block if social tracking protection lists are supplied to {@link
     * #setAntiTracking}.
     *
     * @param enabled A boolean indicating whether or not to enable strict social tracking
     *     protection.
     * @return This Settings instance.
     */
    public @NonNull Settings setStrictSocialTrackingProtection(final boolean enabled) {
      mStStrict.commit(enabled);
      return this;
    }

    /**
     * Set safe browsing categories.
     *
     * @param cat The categories of resources that should be blocked. Use one or more of the {@link
     *     ContentBlocking.SafeBrowsing} flags.
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
      return ContentBlocking.atListToAtCat(mAt.get())
          | ContentBlocking.cmListToAtCat(mCmList.get())
          | ContentBlocking.fpListToAtCat(mFpList.get())
          | ContentBlocking.stListToAtCat(mStList.get());
    }

    /**
     * Get the set ETP behavior level.
     *
     * @return The current ETP level; one of {@link ContentBlocking.EtpLevel}.
     */
    public @CBEtpLevel int getEnhancedTrackingProtectionLevel() {
      if (mEtpStrict.get()) {
        return ContentBlocking.EtpLevel.STRICT;
      } else if (mEtpEnabled.get()) {
        return ContentBlocking.EtpLevel.DEFAULT;
      }
      return ContentBlocking.EtpLevel.NONE;
    }

    /**
     * Get whether or not strict social tracking protection is enabled.
     *
     * @return A boolean indicating whether or not strict social tracking protection is enabled.
     */
    public boolean getStrictSocialTrackingProtection() {
      return mStStrict.get();
    }

    /**
     * Get the set safe browsing categories.
     *
     * @return The categories of resources to be blocked.
     */
    public @CBSafeBrowsing int getSafeBrowsingCategories() {
      return ContentBlocking.sbMalwareToSbCat(mSbMalware.get())
          | ContentBlocking.sbPhishingToSbCat(mSbPhishing.get());
    }

    /**
     * Get the assigned cookie storage behavior.
     *
     * @return The assigned behavior, as one of {@link CookieBehavior} flags.
     */
    @SuppressLint("WrongConstant")
    public @CBCookieBehavior int getCookieBehavior() {
      return mCookieBehavior.get();
    }

    /**
     * Set cookie storage behavior.
     *
     * @param behavior The storage behavior that should be applied. Use one of the {@link
     *     CookieBehavior} flags.
     * @return This Settings instance.
     */
    public @NonNull Settings setCookieBehavior(final @CBCookieBehavior int behavior) {
      mCookieBehavior.commit(behavior);
      return this;
    }

    /**
     * Get the assigned private mode cookie storage behavior.
     *
     * @return The assigned behavior, as one of {@link CookieBehavior} flags.
     */
    @SuppressLint("WrongConstant")
    public @CBCookieBehavior int getCookieBehaviorPrivateMode() {
      return mCookieBehaviorPrivateMode.get();
    }

    /**
     * Set cookie storage behavior for private browsing mode.
     *
     * @param behavior The storage behavior that should be applied. Use one of the {@link
     *     CookieBehavior} flags.
     * @return This Settings instance.
     */
    public @NonNull Settings setCookieBehaviorPrivateMode(final @CBCookieBehavior int behavior) {
      mCookieBehaviorPrivateMode.commit(behavior);
      return this;
    }

    /**
     * Get the assigned cookie lifetime.
     *
     * @return The assigned lifetime, as one of {@link CookieLifetime} flags.
     * @deprecated This feature is not supported anymore. This method always returns 0.
     */
    @Deprecated
    @DeprecationSchedule(id = "cookie-lifetime-policy", version = 106)
    @SuppressLint("WrongConstant")
    public @CBCookieLifetime int getCookieLifetime() {
      return 0;
    }

    /**
     * Set the cookie lifetime.
     *
     * @param lifetime The enforced cookie lifetime. Use one of the {@link CookieLifetime} flags.
     * @return This Settings instance.
     * @deprecated This feature is not supported anymore.
     */
    @Deprecated
    @DeprecationSchedule(id = "cookie-lifetime-policy", version = 106)
    public @NonNull Settings setCookieLifetime(final @CBCookieLifetime int lifetime) {
      return this;
    }

    /**
     * Get whether or not cookie purging is enabled.
     *
     * @return A boolean indicating whether or not cookie purging is enabled.
     */
    public boolean getCookiePurging() {
      return mCookiePurging.get();
    }

    /**
     * Enable or disable cookie purging. This will automatically purge cookies from tracking sites
     * that have no recent user interaction, provided the cookie behavior is set to {@link
     * ContentBlocking.CookieBehavior#ACCEPT_NON_TRACKERS} or {@link
     * ContentBlocking.CookieBehavior#ACCEPT_FIRST_PARTY_AND_ISOLATE_OTHERS}.
     *
     * @param enabled A boolean indicating whether to enable cookie purging.
     * @return This Settings instance.
     */
    public @NonNull Settings setCookiePurging(final boolean enabled) {
      mCookiePurging.commit(enabled);
      return this;
    }

    public static final Parcelable.Creator<Settings> CREATOR =
        new Parcelable.Creator<Settings>() {
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

  /**
   * Holds configuration for a SafeBrowsing provider. <br>
   * <br>
   * This class can be used to modify existing configuration for SafeBrowsing providers or to add a
   * custom SafeBrowsing provider to the app. <br>
   * <br>
   * Default configuration for Google's SafeBrowsing servers can be found at {@link
   * ContentBlocking#GOOGLE_SAFE_BROWSING_PROVIDER} and {@link
   * ContentBlocking#GOOGLE_LEGACY_SAFE_BROWSING_PROVIDER}. <br>
   * <br>
   * This class is immutable, once constructed its values cannot be changed. <br>
   * <br>
   * You can, however, use the {@link #from} method to build upon an existing configuration. For
   * example to override the Google's server configuration, you can do the following: <br>
   *
   * <pre><code>
   *     SafeBrowsingProvider override = SafeBrowsingProvider
   *         .from(ContentBlocking.GOOGLE_SAFE_BROWSING_PROVIDER)
   *         .getHashUrl("http://my-custom-server.com/...")
   *         .updateUrl("http://my-custom-server.com/...")
   *         .build();
   *
   *     runtime.getContentBlocking().setSafeBrowsingProviders(override);
   * </code></pre>
   *
   * This will override the configuration. <br>
   * <br>
   * You can also add a custom SafeBrowsing provider using the {@link #withName} method. For
   * example, to add a custom provider that provides the list <code>testprovider-phish-digest256
   * </code> do the following: <br>
   *
   * <pre><code>
   *     SafeBrowsingProvider custom = SafeBrowsingProvider
   *         .withName("custom-provider")
   *         .version("2.2")
   *         .lists("testprovider-phish-digest256")
   *         .updateUrl("http://my-custom-server2.com/...")
   *         .getHashUrl("http://my-custom-server2.com/...")
   *         .build();
   * </code></pre>
   *
   * And then add the custom provider (adding optionally existing providers): <br>
   *
   * <pre><code>
   *     runtime.getContentBlocking().setSafeBrowsingProviders(
   *         custom,
   *         // Add this if you want to keep the existing configuration too.
   *         ContentBlocking.GOOGLE_SAFE_BROWSING_PROVIDER,
   *         ContentBlocking.GOOGLE_LEGACY_SAFE_BROWSING_PROVIDER);
   * </code></pre>
   *
   * And set the list in the phishing configuration <br>
   *
   * <pre><code>
   *     runtime.getContentBlocking().setSafeBrowsingPhishingTable(
   *          "testprovider-phish-digest256",
   *          // Existing configuration
   *          "goog-phish-proto");
   * </code></pre>
   *
   * Note that any list present in the phishing or malware tables need to appear in one safe
   * browsing provider's {@link #getLists} property.
   *
   * <p>See also <a href="https://developers.google.com/safe-browsing/v4">safe-browsing/v4</a>.
   */
  @AnyThread
  public static class SafeBrowsingProvider extends RuntimeSettings {
    private static final String ROOT = "browser.safebrowsing.provider.";

    private final String mName;

    /* package */ final Pref<String> mVersion;
    /* package */ final Pref<String> mLists;
    /* package */ final Pref<String> mUpdateUrl;
    /* package */ final Pref<String> mGetHashUrl;
    /* package */ final Pref<String> mReportUrl;
    /* package */ final Pref<String> mReportPhishingMistakeUrl;
    /* package */ final Pref<String> mReportMalwareMistakeUrl;
    /* package */ final Pref<String> mAdvisoryUrl;
    /* package */ final Pref<String> mAdvisoryName;
    /* package */ final Pref<String> mDataSharingUrl;
    /* package */ final Pref<Boolean> mDataSharingEnabled;

    /**
     * Creates a {@link SafeBrowsingProvider.Builder} for a provider with the given name.
     *
     * <p>Note: the <code>mozilla</code> name is reserved for internal use, and this method will
     * throw if you attempt to build a provider with that name.
     *
     * @param name The name of the provider.
     * @return a {@link Builder} instance that can be used to build a provider.
     * @throws IllegalArgumentException if this method is called with <code>name="mozilla"</code>
     */
    @NonNull
    public static Builder withName(final @NonNull String name) {
      if ("mozilla".equals(name)) {
        throw new IllegalArgumentException("The 'mozilla' name is reserved for internal use.");
      }
      return new Builder(name);
    }

    /**
     * Creates a {@link SafeBrowsingProvider.Builder} based on the given provider.
     *
     * <p>All properties not otherwise specified will be copied from the provider given in input.
     *
     * @param provider The source provider for this builder.
     * @return a {@link Builder} instance that can be used to create a configuration based on the
     *     builder in input.
     */
    @NonNull
    public static Builder from(final @NonNull SafeBrowsingProvider provider) {
      return new Builder(provider);
    }

    @AnyThread
    public static class Builder {
      final SafeBrowsingProvider mProvider;

      private Builder(final String name) {
        mProvider = new SafeBrowsingProvider(name);
      }

      private Builder(final SafeBrowsingProvider source) {
        mProvider = new SafeBrowsingProvider(source);
      }

      /**
       * Sets the SafeBrowsing protocol session for this provider.
       *
       * @param version the version strong, e.g. "2.2" or "4".
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder version(final @NonNull String version) {
        mProvider.mVersion.set(version);
        return this;
      }

      /**
       * Sets the lists provided by this provider.
       *
       * @param lists one or more lists for this provider, e.g. "goog-malware-proto",
       *     "goog-unwanted-proto"
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder lists(final @NonNull String... lists) {
        mProvider.mLists.set(ContentBlocking.listsToPref(lists));
        return this;
      }

      /**
       * Sets the url that will be used to update the threat list for this provider.
       *
       * <p>See also <a
       * href="https://developers.google.com/safe-browsing/v4/reference/rest/v4/threatListUpdates/fetch">
       * v4/threadListUpdates/fetch </a>.
       *
       * @param updateUrl the update url endpoint for this provider
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder updateUrl(final @NonNull String updateUrl) {
        mProvider.mUpdateUrl.set(updateUrl);
        return this;
      }

      /**
       * Sets the url that will be used to get the full hashes that match a partial hash.
       *
       * <p>See also <a
       * href="https://developers.google.com/safe-browsing/v4/reference/rest/v4/fullHashes/find">
       * v4/fullHashes/find </a>.
       *
       * @param getHashUrl the gethash url endpoint for this provider
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder getHashUrl(final @NonNull String getHashUrl) {
        mProvider.mGetHashUrl.set(getHashUrl);
        return this;
      }

      /**
       * Set the url that will be used to report a url to the SafeBrowsing provider.
       *
       * @param reportUrl the url endpoint to report a url to this provider.
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder reportUrl(final @NonNull String reportUrl) {
        mProvider.mReportUrl.set(reportUrl);
        return this;
      }

      /**
       * Set the url that will be used to report a url mistakenly reported as Phishing to the
       * SafeBrowsing provider.
       *
       * @param reportPhishingMistakeUrl the url endpoint to report a url to this provider.
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder reportPhishingMistakeUrl(
          final @NonNull String reportPhishingMistakeUrl) {
        mProvider.mReportPhishingMistakeUrl.set(reportPhishingMistakeUrl);
        return this;
      }

      /**
       * Set the url that will be used to report a url mistakenly reported as Malware to the
       * SafeBrowsing provider.
       *
       * @param reportMalwareMistakeUrl the url endpoint to report a url to this provider.
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder reportMalwareMistakeUrl(
          final @NonNull String reportMalwareMistakeUrl) {
        mProvider.mReportMalwareMistakeUrl.set(reportMalwareMistakeUrl);
        return this;
      }

      /**
       * Set the url that will be used to give a general advisory about this SafeBrowsing provider.
       *
       * @param advisoryUrl the adivisory page url for this provider.
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder advisoryUrl(final @NonNull String advisoryUrl) {
        mProvider.mAdvisoryUrl.set(advisoryUrl);
        return this;
      }

      /**
       * Set the advisory name for this provider.
       *
       * @param advisoryName the adivisory name for this provider.
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder advisoryName(final @NonNull String advisoryName) {
        mProvider.mAdvisoryName.set(advisoryName);
        return this;
      }

      /**
       * Set url to share threat data to the provider, if enabled by {@link #dataSharingEnabled}.
       *
       * @param dataSharingUrl the url endpoint
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder dataSharingUrl(final @NonNull String dataSharingUrl) {
        mProvider.mDataSharingUrl.set(dataSharingUrl);
        return this;
      }

      /**
       * Set whether to share threat data with the provider, off by default.
       *
       * @param dataSharingEnabled <code>true</code> if the browser should share threat data with
       *     the provider.
       * @return this {@link Builder} instance.
       */
      public @NonNull Builder dataSharingEnabled(final boolean dataSharingEnabled) {
        mProvider.mDataSharingEnabled.set(dataSharingEnabled);
        return this;
      }

      /**
       * Build the {@link SafeBrowsingProvider} based on this {@link Builder} instance.
       *
       * @return thie {@link SafeBrowsingProvider} instance.
       */
      public @NonNull SafeBrowsingProvider build() {
        return new SafeBrowsingProvider(mProvider);
      }
    }

    /* package */ SafeBrowsingProvider(final SafeBrowsingProvider source) {
      this(/* name */ null, /* parent */ null, source);
    }

    /* package */ SafeBrowsingProvider(
        final RuntimeSettings parent, final SafeBrowsingProvider source) {
      this(/* name */ null, parent, source);
    }

    /* package */ SafeBrowsingProvider(final String name) {
      this(name, /* parent */ null, /* source */ null);
    }

    /* package */ SafeBrowsingProvider(
        final String name, final RuntimeSettings parent, final SafeBrowsingProvider source) {
      super(parent);

      if (name != null) {
        mName = name;
      } else if (source != null) {
        mName = source.mName;
      } else {
        throw new IllegalArgumentException("Either name or source must be non-null");
      }

      mVersion = new Pref<>(ROOT + mName + ".pver", null);
      mLists = new Pref<>(ROOT + mName + ".lists", null);
      mUpdateUrl = new Pref<>(ROOT + mName + ".updateURL", null);
      mGetHashUrl = new Pref<>(ROOT + mName + ".gethashURL", null);
      mReportUrl = new Pref<>(ROOT + mName + ".reportURL", null);
      mReportPhishingMistakeUrl = new Pref<>(ROOT + mName + ".reportPhishMistakeURL", null);
      mReportMalwareMistakeUrl = new Pref<>(ROOT + mName + ".reportMalwareMistakeURL", null);
      mAdvisoryUrl = new Pref<>(ROOT + mName + ".advisoryURL", null);
      mAdvisoryName = new Pref<>(ROOT + mName + ".advisoryName", null);
      mDataSharingUrl = new Pref<>(ROOT + mName + ".dataSharingURL", null);
      mDataSharingEnabled = new Pref<>(ROOT + mName + ".dataSharing.enabled", false);

      if (source != null) {
        updatePrefs(source);
      }
    }

    /**
     * Get the name of this provider.
     *
     * @return a string containing the name.
     */
    public @NonNull String getName() {
      return mName;
    }

    /**
     * Get the version for this provider.
     *
     * @return a string representing the version, e.g. "2.2" or "4".
     */
    public @Nullable String getVersion() {
      return mVersion.get();
    }

    /**
     * Get the lists provided by this provider.
     *
     * @return an array of string identifiers for the lists
     */
    public @NonNull String[] getLists() {
      return ContentBlocking.prefToLists(mLists.get());
    }

    /**
     * Get the url that will be used to update the threat list for this provider.
     *
     * <p>See also <a
     * href="https://developers.google.com/safe-browsing/v4/reference/rest/v4/threatListUpdates/fetch">
     * v4/threadListUpdates/fetch </a>.
     *
     * @return a string containing the URL.
     */
    public @Nullable String getUpdateUrl() {
      return mUpdateUrl.get();
    }

    /**
     * Get the url that will be used to get the full hashes that match a partial hash.
     *
     * <p>See also <a
     * href="https://developers.google.com/safe-browsing/v4/reference/rest/v4/fullHashes/find">
     * v4/fullHashes/find </a>.
     *
     * @return a string containing the URL.
     */
    public @Nullable String getGetHashUrl() {
      return mGetHashUrl.get();
    }

    /**
     * Get the url that will be used to report a url to the SafeBrowsing provider.
     *
     * @return a string containing the URL.
     */
    public @Nullable String getReportUrl() {
      return mReportUrl.get();
    }

    /**
     * Get the url that will be used to report a url mistakenly reported as Phishing to the
     * SafeBrowsing provider.
     *
     * @return a string containing the URL.
     */
    public @Nullable String getReportPhishingMistakeUrl() {
      return mReportPhishingMistakeUrl.get();
    }

    /**
     * Get the url that will be used to report a url mistakenly reported as Malware to the
     * SafeBrowsing provider.
     *
     * @return a string containing the URL.
     */
    public @Nullable String getReportMalwareMistakeUrl() {
      return mReportMalwareMistakeUrl.get();
    }

    /**
     * Get the url that will be used to give a general advisory about this SafeBrowsing provider.
     *
     * @return a string containing the URL.
     */
    public @Nullable String getAdvisoryUrl() {
      return mAdvisoryUrl.get();
    }

    /**
     * Get the advisory name for this provider.
     *
     * @return a string containing the URL.
     */
    public @Nullable String getAdvisoryName() {
      return mAdvisoryName.get();
    }

    /**
     * Get the url to share threat data to the provider, if enabled by {@link
     * #getDataSharingEnabled}.
     *
     * @return this {@link Builder} instance.
     */
    public @Nullable String getDataSharingUrl() {
      return mDataSharingUrl.get();
    }

    /**
     * Get whether to share threat data with the provider.
     *
     * @return <code>true</code> if the browser should whare threat data with the provider, <code>
     *     false</code> otherwise.
     */
    public @Nullable Boolean getDataSharingEnabled() {
      return mDataSharingEnabled.get();
    }

    @Override // Parcelable
    @AnyThread
    public void writeToParcel(final Parcel out, final int flags) {
      out.writeValue(mName);
      super.writeToParcel(out, flags);
    }

    /** Creator instance for this class. */
    public static final Parcelable.Creator<SafeBrowsingProvider> CREATOR =
        new Parcelable.Creator<SafeBrowsingProvider>() {
          @Override
          public SafeBrowsingProvider createFromParcel(final Parcel source) {
            final String name = (String) source.readValue(getClass().getClassLoader());
            final SafeBrowsingProvider settings = new SafeBrowsingProvider(name);
            settings.readFromParcel(source);
            return settings;
          }

          @Override
          public SafeBrowsingProvider[] newArray(final int size) {
            return new SafeBrowsingProvider[size];
          }
        };
  }

  private static String listsToPref(final String... lists) {
    final StringBuilder prefBuilder = new StringBuilder();

    for (final String list : lists) {
      if (list.contains(",")) {
        // We use ',' as the separator, so the list name cannot contain it.
        // Should never happen.
        throw new IllegalArgumentException("List name cannot contain ',' character.");
      }

      prefBuilder.append(list);
      prefBuilder.append(",");
    }

    // Remove trailing ","
    if (lists.length > 0) {
      prefBuilder.setLength(prefBuilder.length() - 1);
    }

    return prefBuilder.toString();
  }

  private static String[] prefToLists(final String pref) {
    return pref != null ? pref.split(",") : new String[] {};
  }

  public static class AntiTracking {
    public static final int NONE = 0;

    /** Block advertisement trackers. */
    public static final int AD = 1 << 1;

    /** Block analytics trackers. */
    public static final int ANALYTIC = 1 << 2;

    /**
     * Block social trackers. Note: This is not the same as "Social Tracking Protection", which is
     * controlled by {@link #STP}.
     */
    public static final int SOCIAL = 1 << 3;

    /** Block content trackers. May cause issues with some web sites. */
    public static final int CONTENT = 1 << 4;

    /** Block Gecko test trackers (used for tests). */
    public static final int TEST = 1 << 5;

    /** Block cryptocurrency miners. */
    public static final int CRYPTOMINING = 1 << 6;

    /** Block fingerprinting trackers. */
    public static final int FINGERPRINTING = 1 << 7;

    /** Block trackers on the Social Tracking Protection list. */
    public static final int STP = 1 << 8;

    /** Block ad, analytic, social and test trackers. */
    public static final int DEFAULT = AD | ANALYTIC | SOCIAL | TEST;

    /** Block all known trackers. May cause issues with some web sites. */
    public static final int STRICT = DEFAULT | CONTENT | CRYPTOMINING | FINGERPRINTING;

    protected AntiTracking() {}
  }

  @Retention(RetentionPolicy.SOURCE)
  @IntDef(
      flag = true,
      value = {
        AntiTracking.AD,
        AntiTracking.ANALYTIC,
        AntiTracking.SOCIAL,
        AntiTracking.CONTENT,
        AntiTracking.TEST,
        AntiTracking.CRYPTOMINING,
        AntiTracking.FINGERPRINTING,
        AntiTracking.DEFAULT,
        AntiTracking.STRICT,
        AntiTracking.STP,
        AntiTracking.NONE
      })
  public @interface CBAntiTracking {}

  public static class SafeBrowsing {
    public static final int NONE = 0;

    /** Block malware sites. */
    public static final int MALWARE = 1 << 10;

    /** Block unwanted sites. */
    public static final int UNWANTED = 1 << 11;

    /** Block harmful sites. */
    public static final int HARMFUL = 1 << 12;

    /** Block phishing sites. */
    public static final int PHISHING = 1 << 13;

    /** Block all unsafe sites. */
    public static final int DEFAULT = MALWARE | UNWANTED | HARMFUL | PHISHING;

    protected SafeBrowsing() {}
  }

  @Retention(RetentionPolicy.SOURCE)
  @IntDef(
      flag = true,
      value = {
        SafeBrowsing.MALWARE, SafeBrowsing.UNWANTED,
        SafeBrowsing.HARMFUL, SafeBrowsing.PHISHING,
        SafeBrowsing.DEFAULT, SafeBrowsing.NONE
      })
  public @interface CBSafeBrowsing {}

  // Sync values with nsICookieService.idl.
  public static class CookieBehavior {
    /** Accept first-party and third-party cookies and site data. */
    public static final int ACCEPT_ALL = 0;

    /**
     * Accept only first-party cookies and site data to block cookies which are not associated with
     * the domain of the visited site.
     */
    public static final int ACCEPT_FIRST_PARTY = 1;

    /** Do not store any cookies and site data. */
    public static final int ACCEPT_NONE = 2;

    /**
     * Accept first-party and third-party cookies and site data only from sites previously visited
     * in a first-party context.
     */
    public static final int ACCEPT_VISITED = 3;

    /**
     * Accept only first-party and non-tracking third-party cookies and site data to block cookies
     * which are not associated with the domain of the visited site set by known trackers.
     */
    public static final int ACCEPT_NON_TRACKERS = 4;

    /**
     * Enable dynamic first party isolation (dFPI); this will block third-party tracking cookies in
     * accordance with the ETP level and isolate non-tracking third-party cookies.
     */
    public static final int ACCEPT_FIRST_PARTY_AND_ISOLATE_OTHERS = 5;

    protected CookieBehavior() {}
  }

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({
    CookieBehavior.ACCEPT_ALL, CookieBehavior.ACCEPT_FIRST_PARTY,
    CookieBehavior.ACCEPT_NONE, CookieBehavior.ACCEPT_VISITED,
    CookieBehavior.ACCEPT_NON_TRACKERS
  })
  public @interface CBCookieBehavior {}

  // Sync values with nsICookieService.idl.
  // This feature is not supported anymore.
  @Deprecated
  @DeprecationSchedule(id = "cookie-lifetime-policy", version = 106)
  public static class CookieLifetime {
    /** Accept default cookie lifetime. */
    public static final int NORMAL = 0;

    /** Downgrade cookie lifetime to this runtime's lifetime. */
    public static final int RUNTIME = 2;

    /** Limit cookie lifetime to N days. Defaults to 90 days. */
    public static final int DAYS = 3;

    protected CookieLifetime() {}
  }

  // This feature is not supported anymore.
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({CookieLifetime.NORMAL, CookieLifetime.RUNTIME, CookieLifetime.DAYS})
  @Deprecated
  @DeprecationSchedule(id = "cookie-lifetime-policy", version = 106)
  public @interface CBCookieLifetime {}

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({EtpLevel.NONE, EtpLevel.DEFAULT, EtpLevel.STRICT})
  public @interface CBEtpLevel {}

  /** Possible settings for ETP. */
  public static class EtpLevel {
    /** Do not enable ETP at all. */
    public static final int NONE = 0;

    /** Enable ETP for ads, analytic, and social tracking lists. */
    public static final int DEFAULT = 1;

    /**
     * Enable ETP for all of the default lists as well as the content list. May break many sites!
     */
    public static final int STRICT = 2;
  }

  /** Holds content block event details. */
  public static class BlockEvent {
    /** The URI of the blocked resource. */
    public final @NonNull String uri;

    private final @CBAntiTracking int mAntiTrackingCat;
    private final @CBSafeBrowsing int mSafeBrowsingCat;
    private final @CBCookieBehavior int mCookieBehaviorCat;
    private final boolean mIsBlocking;

    @SuppressWarnings("checkstyle:javadocmethod")
    public BlockEvent(
        @NonNull final String uri,
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
     *
     * @return One or more of the {@link AntiTracking} flags.
     */
    @UiThread
    public @CBAntiTracking int getAntiTrackingCategory() {
      return mAntiTrackingCat;
    }

    /**
     * The safe browsing category types of the blocked resource.
     *
     * @return One or more of the {@link SafeBrowsing} flags.
     */
    @UiThread
    public @CBSafeBrowsing int getSafeBrowsingCategory() {
      return mSafeBrowsingCat;
    }

    /**
     * The cookie types of the blocked resource.
     *
     * @return One or more of the {@link CookieBehavior} flags.
     */
    @UiThread
    public @CBCookieBehavior int getCookieBehaviorCategory() {
      return mCookieBehaviorCat;
    }

    /* package */ static BlockEvent fromBundle(@NonNull final GeckoBundle bundle) {
      final String uri = bundle.getString("uri");
      final String blockedList = bundle.getString("blockedList");
      final String loadedList = TextUtils.join(",", bundle.getStringArray("loadedLists"));
      final long error = bundle.getLong("error", 0L);
      final long category = bundle.getLong("category", 0L);

      final String matchedList = blockedList != null ? blockedList : loadedList;

      // Note: Even if loadedList is non-empty it does not necessarily
      // mean that the event is not a blocking event.
      final boolean blocking =
          (blockedList != null || error != 0L || ContentBlocking.isBlockingGeckoCbCat(category));

      return new BlockEvent(
          uri,
          ContentBlocking.atListToAtCat(matchedList)
              | ContentBlocking.cmListToAtCat(matchedList)
              | ContentBlocking.fpListToAtCat(matchedList)
              | ContentBlocking.stListToAtCat(matchedList),
          ContentBlocking.errorToSbCat(error),
          ContentBlocking.geckoCatToCbCat(category),
          blocking);
    }

    @UiThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public boolean isBlocking() {
      return mIsBlocking;
    }
  }

  /** GeckoSession applications implement this interface to handle content blocking events. */
  public interface Delegate {
    /**
     * A content element has been blocked from loading. Set blocked element categories via {@link
     * GeckoRuntimeSettings} and enable content blocking via {@link GeckoSessionSettings}.
     *
     * @param session The GeckoSession that initiated the callback.
     * @param event The {@link BlockEvent} details.
     */
    @UiThread
    default void onContentBlocked(
        @NonNull final GeckoSession session, @NonNull final BlockEvent event) {}

    /**
     * A content element that could be blocked has been loaded.
     *
     * @param session The GeckoSession that initiated the callback.
     * @param event The {@link BlockEvent} details.
     */
    @UiThread
    default void onContentLoaded(
        @NonNull final GeckoSession session, @NonNull final BlockEvent event) {}
  }

  private static final String TEST = "moztest-track-simple";
  private static final String AD = "ads-track-digest256";
  private static final String ANALYTIC = "analytics-track-digest256";
  private static final String SOCIAL = "social-track-digest256";
  private static final String CONTENT = "content-track-digest256";
  private static final String CRYPTOMINING = "base-cryptomining-track-digest256";
  private static final String FINGERPRINTING = "base-fingerprinting-track-digest256";
  private static final String STP =
      "social-tracking-protection-facebook-digest256,social-tracking-protection-linkedin-digest256,social-tracking-protection-twitter-digest256";

  /* package */ static @CBSafeBrowsing int sbMalwareToSbCat(final boolean enabled) {
    return enabled
        ? (SafeBrowsing.MALWARE | SafeBrowsing.UNWANTED | SafeBrowsing.HARMFUL)
        : SafeBrowsing.NONE;
  }

  /* package */ static @CBSafeBrowsing int sbPhishingToSbCat(final boolean enabled) {
    return enabled ? SafeBrowsing.PHISHING : SafeBrowsing.NONE;
  }

  /* package */ static boolean catToSbMalware(@CBAntiTracking final int cat) {
    return (cat & (SafeBrowsing.MALWARE | SafeBrowsing.UNWANTED | SafeBrowsing.HARMFUL)) != 0;
  }

  /* package */ static boolean catToSbPhishing(@CBAntiTracking final int cat) {
    return (cat & SafeBrowsing.PHISHING) != 0;
  }

  /* package */ static String catToAtPref(@CBAntiTracking final int cat) {
    final StringBuilder builder = new StringBuilder();

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
    final StringBuilder builder = new StringBuilder();

    if ((cat & AntiTracking.CRYPTOMINING) != 0) {
      builder.append(CRYPTOMINING);
    }
    return builder.toString();
  }

  /* package */ static boolean catToFpPref(@CBAntiTracking final int cat) {
    return (cat & AntiTracking.FINGERPRINTING) != 0;
  }

  /* package */ static String catToFpListPref(@CBAntiTracking final int cat) {
    final StringBuilder builder = new StringBuilder();

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

  /* package */ static boolean catToStPref(@CBAntiTracking final int cat) {
    return (cat & AntiTracking.STP) != 0;
  }

  /* package */ static String catToStListPref(@CBAntiTracking final int cat) {
    final StringBuilder builder = new StringBuilder();

    if ((cat & AntiTracking.STP) != 0) {
      builder.append(STP).append(",");
    }
    if (builder.length() == 0) {
      return "";
    }
    // Trim final ','.
    return builder.substring(0, builder.length() - 1);
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

  /* package */ static @CBAntiTracking int stListToAtCat(final String list) {
    int cat = AntiTracking.NONE;
    if (list == null) {
      return cat;
    }
    if (list.indexOf(STP) != -1) {
      cat |= AntiTracking.STP;
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
  private static final long STATE_COOKIES_LOADED_TRACKER = 0x40000L;
  private static final long STATE_COOKIES_LOADED_SOCIALTRACKER = 0x80000L;
  private static final long STATE_COOKIES_BLOCKED_TRACKER = 0x20000000L;
  private static final long STATE_COOKIES_BLOCKED_SOCIALTRACKER = 0x01000000L;
  private static final long STATE_COOKIES_BLOCKED_ALL = 0x40000000L;
  private static final long STATE_COOKIES_BLOCKED_FOREIGN = 0x80L;

  /* package */ static boolean isBlockingGeckoCbCat(final long geckoCat) {
    return (geckoCat
            & (STATE_COOKIES_BLOCKED_TRACKER
                | STATE_COOKIES_BLOCKED_SOCIALTRACKER
                | STATE_COOKIES_BLOCKED_ALL
                | STATE_COOKIES_BLOCKED_FOREIGN))
        != 0;
  }

  /* package */ static @CBCookieBehavior int geckoCatToCbCat(final long geckoCat) {
    if ((geckoCat & STATE_COOKIES_LOADED) != 0) {
      // We don't know which setting would actually block this cookie, so
      // we return the most strict value.
      return CookieBehavior.ACCEPT_NONE;
    }
    if ((geckoCat & STATE_COOKIES_BLOCKED_FOREIGN) != 0) {
      return CookieBehavior.ACCEPT_FIRST_PARTY;
    }
    // If we receive STATE_COOKIES_LOADED_{SOCIAL,}TRACKER we know that this
    // setting would block this cookie.
    if ((geckoCat
            & (STATE_COOKIES_BLOCKED_TRACKER
                | STATE_COOKIES_BLOCKED_SOCIALTRACKER
                | STATE_COOKIES_LOADED_TRACKER
                | STATE_COOKIES_LOADED_SOCIALTRACKER))
        != 0) {
      return CookieBehavior.ACCEPT_NON_TRACKERS;
    }
    if ((geckoCat & STATE_COOKIES_BLOCKED_ALL) != 0) {
      return CookieBehavior.ACCEPT_NONE;
    }
    // TODO: There are more reasons why cookies may be blocked.
    return CookieBehavior.ACCEPT_ALL;
  }
}
