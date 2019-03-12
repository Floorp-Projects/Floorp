/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components.metrics

import android.app.Application
import com.leanplum.Leanplum
import com.leanplum.LeanplumActivityHelper
import com.leanplum.annotations.Parser
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.utils.Settings

private val Event.name: String
    get() = when (this) {
    is Event.AddBookmark -> "E_Add_Bookmark"
    is Event.RemoveBookmark -> "E_Remove_Bookmark"
    is Event.OpenedBookmark -> "E_Opened_Bookmark"
    is Event.OpenedApp -> "E_Opened_App"
    is Event.OpenedAppFirstRun -> "E_Opened_App_FirstRun"
    is Event.InteractWithSearchURLArea -> "E_Interact_With_Search_URL_Area"
    is Event.SavedLoginandPassword -> "E_Saved_Login_and_Password"
    is Event.FXANewSignup -> "E_FXA_New_Signup"
    is Event.UserSignedInToFxA -> "E_User_Signed_In_To_FxA"
    is Event.UserDownloadedFocus -> "E_User_Downloaded_Focus"
    is Event.UserDownloadedLockbox -> "E_User_Downloaded_Lockbox"
    is Event.UserDownloadedFennec -> "E_User_Downloaded_Fennec"
    is Event.TrackingProtectionSettingsChanged -> "E_Tracking_Protection_Settings_Changed"
    is Event.FXASyncedNewDevice -> "E_FXA_Synced_New_Device"
    is Event.DismissedOnboarding -> "E_Dismissed_Onboarding"
    is Event.Uninstall -> "E_Uninstall"
    is Event.OpenNewNormalTab -> "E_Open_New_Normal_Tab"
    is Event.OpenNewPrivateTab -> "E_Open_New_Private_Tab"
    is Event.ShareStarted -> "E_Share_Started"
    is Event.ShareCanceled -> "E_Share_Canceled"
    is Event.ShareCompleted -> "E_Share_Completed"
    is Event.ClosePrivateTabs -> "E_Close_Private_Tabs"
    is Event.ClearedPrivateData -> "E_Cleared_Private_Data"
    is Event.OpenedLoginManager -> "E_Opened_Login_Manager"
    is Event.OpenedMailtoLink -> "E_Opened_Mailto_Link"
    is Event.DownloadMediaSavedImage -> "E_Download_Media_Saved_Image"
    is Event.UserUsedReaderView -> "E_User_Used_Reader_View"
    is Event.UserDownloadedPocket -> "E_User_Downloaded_Pocket"
    is Event.UserDownloadedSend -> "E_User_Downloaded_Send"
    is Event.OpenedPocketStory -> "E_Opened_Pocket_Story"
    is Event.DarkModeEnabled -> "E_Dark_Mode_Enabled"
}

class LeanplumMetricsService(private val application: Application) : MetricsService {
    override fun start() {
        Leanplum.setApplicationContext(application)
        Parser.parseVariables(application)
        LeanplumActivityHelper.enableLifecycleCallbacks(application)
        Leanplum.setAppIdForProductionMode(LeanplumId, LeanplumToken)
        Leanplum.start(application)
    }

    override fun track(event: Event) {
        Leanplum.track(event.name, event.extras)
    }

    override fun shouldTrack(event: Event): Boolean = Settings.getInstance(application).isTelemetryEnabled

    companion object {
        private val LeanplumId: String
            get() = BuildConfig.LEANPLUM_ID ?: ""
        private val LeanplumToken: String
            get() = BuildConfig.LEANPLUM_TOKEN ?: ""
    }
}
