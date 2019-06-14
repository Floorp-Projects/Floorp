/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.support.annotation.NonNull;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

import java.util.Locale;

/**
 * Helper class that ensures the Onboarding screens will always use localized text.
 *
 * <ul>There are currently 3 scenarios that we support:<br>
 *      <li>Old Onboarding</li>
 *          If the Strings populating the Onboarding screens are not translated.
 *      <li>Updated Onboarding. New images, new Strings. Bug 1545805</li>
 *          If all the Strings populating the Onboarding screens are translated in the current locale.
 *      <li>Updated Sync Onboarding screen. Renamed to Account. New Image, new Strings. Bug 1557635</li>
 *          If the Sync screen's title, message and sync button's text is translated.
 * </ul>
 *
 * <ul>To be able to distinguish between them we are using different prefixes.<br>
 *     From oldest to newest:
 *     <li>initial</li>
 *     <li>new</li>
 *     <li>updated</li>
 * </ul>
 */
public class OnboardingResources {
    private static OnboardingResources INSTANCE;

    /**
     * What Onboarding version should the user see
     */
    public enum Version {
        /**
         * Original onboarding, used until Fennec v67.
         */
        INITIAL,
        /**
         * Updated onboarding. Bug 1545805.
         * <li>only 3 screens max. No more "Customize" screen.</li>
         * <li>only the "Welcome" screen will use an updated message. The others will show just the subtext.</li>
         */
        NEW,
        /**
         * Updated onboarding + updated "Sync" screen, now "Account". Bug 1557635.
         * <li>the title of the "Sync" screen will be "Account"</li>
         * <li>the subtext and the sync button text will also be updated</li>
         */
        UPDATED,
    }

    // Whether the resources for the updated onboarding UX are localized or not.
    private boolean areStringsLocalized;
    // The Onboarding version that should be displayed
    private Version currentVersion;

    private String[] welcomeMessages;
    private String[] welcomeSubtexts;
    private String[] privacySubtexts;
    private String[] syncTitles;
    private String[] syncSubtexts;
    private String[] syncAccountButtons;
    private int[] syncImageResIds;

    private OnboardingResources(final Context context) {
        initializeResources(context);

        setOnboardingVersion(context);
    }

    // Simple Singleton as we don't expect concurrency issues.
    public static OnboardingResources getInstance(final Context context) {
        if (INSTANCE == null) {
            INSTANCE = new OnboardingResources(context);
        }
        return INSTANCE;
    }

    /**
     * Get if the new Onboarding UX has it's resources localized.
     */
    public boolean useNewOnboarding() {
        return areStringsLocalized;
    }

    /**
     * Get the Onboarding version that should be used depending on the localization version.
     */
    public Version getVersion() {
        return currentVersion;
    }

    public String getWelcomeMessage() {
        return welcomeMessages[currentVersion.ordinal()];
    }

    public String getWelcomeSubtext() {
        return welcomeSubtexts[currentVersion.ordinal()];
    }

    public String getPrivacySubtext() {
        return privacySubtexts[currentVersion.ordinal()];
    }

    public String getSyncTitle() {
        return syncTitles[currentVersion.ordinal()];
    }

    public String getSyncSubtext() {
        return syncSubtexts[currentVersion.ordinal()];
    }

    public String getSyncButtonText() {
        return syncAccountButtons[currentVersion.ordinal()];
    }

    public int getSyncImageResId() {
        return syncImageResIds[currentVersion.ordinal()];
    }

    /**
     * Establish the Onboarding version that the user should see by comparing the
     * localized String resources witht their default english values.
     */
    private void setOnboardingVersion(@NonNull final Context context) {
        final Locale locale = context.getResources().getConfiguration().locale;

        // If current locale is the default "en" we'll show the newest Onboarding UX
        if ("en".equals(locale.getLanguage())) {
            currentVersion = Version.UPDATED;
            areStringsLocalized = true;
            return;
        }

        final int newStringIndex = Version.NEW.ordinal();
        final int updatedStringIndex = Version.UPDATED.ordinal();

        // Check if the second version of Onboarding has everything localized
        // by checking if the "newfirstrun" prefixed strings have a different value than the default english one.
        final String englishWelcomeMessage = "Welcome to " + AppConstants.MOZ_APP_BASENAME;
        final String englishWelcomeSubtext = "A modern mobile browser from Mozilla, the non-profit committed to a free and open web.";
        final boolean areWelcomeStringsLocalized =
                !englishWelcomeMessage.equals(welcomeMessages[newStringIndex]) &&
                !englishWelcomeSubtext.equals(welcomeSubtexts[newStringIndex]);

        final String englishPrivacySubtext = "Private browsing blocks ad trackers that follow you online.";
        final boolean arePrivacyStringsLocalized =
                !englishPrivacySubtext.equals(privacySubtexts[newStringIndex]);

        final String englishSyncSubtext = "Sync the things you save on mobile to Firefox for desktop, privately and securely.";
        final String englishSyncButton = "Turn on Sync";
        final boolean areSyncStringsLocalized =
                !englishSyncSubtext.equals(syncSubtexts[newStringIndex]) &&
                !englishSyncButton.equals(syncAccountButtons[newStringIndex]);

        areStringsLocalized = areWelcomeStringsLocalized && arePrivacyStringsLocalized && areSyncStringsLocalized;
        if (!areStringsLocalized) {
            // Default to showing the old Onboarding since the new UX doesn't have all resource localized.
            currentVersion = Version.INITIAL;
        } else {
            // We can show the new Onboarding.
            currentVersion = Version.NEW;

            // Check if we can use the updated Strings for the updated Sync onboarding screen
            // Ignore checking if the new title - "Account" has been translated as it is a single word
            // that could erroneously result in a false negative response.
            final String englishSyncUpdatedSubtext = "Sign in to your account to get the most out of " + AppConstants.MOZ_APP_BASENAME;
            final String englishSyncUpdatedButton = "Sign in to " + AppConstants.MOZ_APP_BASENAME;
            final boolean areUpdatedSyncStringsLocalized =
                    !englishSyncUpdatedSubtext.equals(syncSubtexts[updatedStringIndex]) &&
                    !englishSyncUpdatedButton.equals(syncAccountButtons[updatedStringIndex]);

            if (areUpdatedSyncStringsLocalized) {
                currentVersion = Version.UPDATED;
            }
        }
    }

    /**
     * Initialize the lookup map for Onboarding string resources.
     */
    private void initializeResources(@NonNull final Context context) {
        welcomeMessages = new String[] {
                context.getString(R.string.firstrun_urlbar_message),
                context.getString(R.string.newfirstrun_urlbar_message),
                context.getString(R.string.newfirstrun_urlbar_message),
        };
        welcomeSubtexts = new String[] {
                context.getString(R.string.firstrun_urlbar_subtext),
                context.getString(R.string.newfirstrun_urlbar_subtext),
                context.getString(R.string.newfirstrun_urlbar_subtext),
        };

        privacySubtexts = new String[] {
                context.getString(R.string.firstrun_privacy_subtext),
                context.getString(R.string.newfirstrun_privacy_subtext),
                context.getString(R.string.newfirstrun_privacy_subtext),
        };

        syncTitles = new String[] {
                context.getString(R.string.firstrun_sync_title),
                context.getString(R.string.firstrun_sync_title),
                context.getString(R.string.updatednewfirstrun_sync_title),
        };

        syncSubtexts = new String[] {
                context.getString(R.string.firstrun_sync_subtext),
                context.getString(R.string.newfirstrun_sync_subtext),
                context.getString(R.string.updatednewfirstrun_sync_subtext),
        };

        syncAccountButtons = new String[] {
                context.getString(R.string.firstrun_signin_button),
                context.getString(R.string.newfirstrun_signin_button),
                context.getString(R.string.updatednewfirstrun_signin_button),
        };

        syncImageResIds = new int[] {
                R.drawable.firstrun_sync,
                R.drawable.firstrun_sync2,
                R.drawable.firstrun_account,
        };
    }
}
