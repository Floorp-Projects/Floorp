/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.accounts

import android.os.Parcel
import android.os.Parcelable
import mozilla.components.concept.sync.FxAEntryPoint

/**
 * Fenix implementation of [FxAEntryPoint].
 */
enum class FenixFxAEntryPoint(override val entryName: String) : FxAEntryPoint, Parcelable {
    /**
     * New user onboarding, the user accessed the sign in through new user onboarding
     */
    NewUserOnboarding("newuser-onboarding"),

    /**
     * Manual sign in from the onboarding menu
     */
    OnboardingManualSignIn("onboarding-manual-sign-in"),

    /**
     * User used a deep link to get to firefox accounts authentication
     */
    DeepLink("deep-link"),

    /**
     * Authenticating from the browser's toolbar
     */
    BrowserToolbar("browser-toolbar"),

    /**
     * Authenticating from the home menu (the hamburger menu)
     */
    HomeMenu("home-menu"),

    /**
     * Authenticating in the bookmark view, when getting attempting to get synced
     * bookmarks
     */
    BookmarkView("bookmark-view"),

    /**
     * Authenticating from the homepage onboarding dialog
     */
    HomeOnboardingDialog("home-onboarding-dialog"),

    /**
     * Authenticating from the settings menu
     */
    SettingsMenu("settings-menu"),

    /**
     * Authenticating from the autofill settings to enable synced
     * credit cards/addresses
     */
    AutofillSetting("autofill-setting"),

    /**
     * Authenticating from the saved logins menu to enable synced
     * logins
     */
    SavedLogins("saved-logins"),

    /**
     * Authenticating from the Share menu to enable send tab
     */
    ShareMenu("share-menu"),

    /**
     * Authenticating as a navigation interaction
     */
    NavigationInteraction("navigation-interaction"),

    /**
     * Authenticating from the synced tabs menu to enable synced tabs
     */
    SyncedTabsMenu("synced-tabs-menu"),

    /**
     * When serializing the value after navigating, the result is a nullable value. We have this
     * "unknown" as a default value in the odd chance that we receive an [entryName] is not part of this enum.
     *
     * Do not use within app code.
     */
    Unknown("unknown"),
    ;

    override fun writeToParcel(parcel: Parcel, flags: Int) {
        parcel.writeString(entryName)
    }

    override fun describeContents(): Int {
        return 0
    }

    /**
     * Override implementation of the [Parcelable.Creator].
     *
     * Implementation notes: We need to manually create an override for [Parcelable] instead of using the annotation,
     * because this is an enum implementation of the API and the auto-generated code does not know how to choose a
     * particular enum value in [Parcelable.Creator.createFromParcel].
     * We also introduce an [FxAEntryPoint.Unknown] value to use as a default return value in the off-chance that we
     * cannot safely serialize the enum value from the navigation library; this should be a rare case, if any.
     */
    companion object CREATOR : Parcelable.Creator<FenixFxAEntryPoint> {
        override fun createFromParcel(parcel: Parcel): FenixFxAEntryPoint {
            val parcelEntryName = parcel.readString() ?: Unknown
            return FenixFxAEntryPoint.values().first { it.entryName == parcelEntryName }
        }

        override fun newArray(size: Int): Array<FenixFxAEntryPoint?> {
            return arrayOfNulls(size)
        }
    }
}
