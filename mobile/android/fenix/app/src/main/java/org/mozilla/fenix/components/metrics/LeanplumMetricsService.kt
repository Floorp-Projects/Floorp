/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.app.Application
import android.util.Log
import com.leanplum.Leanplum
import com.leanplum.LeanplumActivityHelper
import com.leanplum.annotations.Parser
import com.leanplum.internal.LeanplumInternal
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.utils.Settings
import java.util.UUID.randomUUID

private val Event.name: String?
    get() = when (this) {
        is Event.AddBookmark -> "E_Add_Bookmark"
        is Event.RemoveBookmark -> "E_Remove_Bookmark"
        is Event.OpenedBookmark -> "E_Opened_Bookmark"
        is Event.OpenedApp -> "E_Opened_App"
        is Event.OpenedAppFirstRun -> "E_Opened_App_FirstRun"
        is Event.InteractWithSearchURLArea -> "E_Interact_With_Search_URL_Area"
        is Event.CollectionSaved -> "E_Collection_Created"
        is Event.CollectionTabRestored -> "E_Collection_Tab_Opened"
        is Event.SyncAuthSignIn -> "E_Sign_In_FxA"
        is Event.SyncAuthSignOut -> "E_Sign_Out_FxA"
        is Event.FXANewSignup -> "E_New_Sign_Up_FxA"
        is Event.ClearedPrivateData -> "E_Cleared_Private_Data"
        is Event.DismissedOnboarding -> "E_Dismissed_Onboarding"

        // Do not track other events in Leanplum
        else -> ""
    }

class LeanplumMetricsService(private val application: Application) : MetricsService {
    data class Token(val id: String, val token: String) {
        enum class Type { Development, Production, Invalid }

        val type by lazy {
            when {
                token.take(ProdPrefix.length) == ProdPrefix -> Type.Production
                token.take(DevPrefix.length) == DevPrefix -> Type.Development
                else -> Type.Invalid
            }
        }

        companion object {
            private const val ProdPrefix = "prod"
            private const val DevPrefix = "dev"
        }
    }

    private val token = Token(LeanplumId, LeanplumToken)

    override fun start() {
        when (token.type) {
            Token.Type.Production -> Leanplum.setAppIdForProductionMode(token.id, token.token)
            Token.Type.Development -> Leanplum.setAppIdForDevelopmentMode(token.id, token.token)
            Token.Type.Invalid -> {
                Log.i(LOGTAG, "Invalid or missing Leanplum token")
                return
            }
        }

        Leanplum.setIsTestModeEnabled(false)
        Leanplum.setApplicationContext(application)
        Leanplum.setDeviceId(randomUUID().toString())
        Parser.parseVariables(application)

        LeanplumActivityHelper.enableLifecycleCallbacks(application)

        val installedApps = MozillaProductDetector.getInstalledMozillaProducts(application)

        Leanplum.start(application, hashMapOf(
            "default_browser" to MozillaProductDetector.getMozillaBrowserDefault(application).orEmpty(),
            "fennec_installed" to installedApps.contains(MozillaProductDetector.MozillaProducts.FIREFOX.productName),
            "focus_installed" to installedApps.contains(MozillaProductDetector.MozillaProducts.FOCUS.productName),
            "klar_installed" to installedApps.contains(MozillaProductDetector.MozillaProducts.KLAR.productName),
            "fxa_signed_in" to Settings.getInstance(application).fxaSignedIn,
            "fxa_has_synced_items" to Settings.getInstance(application).fxaHasSyncedItems,
            "search_widget_installed" to Settings.getInstance(application).searchWidgetInstalled
        ))
    }

    override fun stop() {
        // As written in LeanPlum SDK documentation, "This prevents Leanplum from communicating with the server."
        // as this "isTestMode" flag is checked before LeanPlum SDK does anything.
        // Also has the benefit effect of blocking the display of already downloaded messages.
        // The reverse of this - setIsTestModeEnabled(false) must be called before trying to init
        // LP in the same session.
        Leanplum.setIsTestModeEnabled(true)

        // This is just to allow restarting LP and it's functionality in the same app session
        // as LP stores it's state internally and check against it
        LeanplumInternal.setCalledStart(false)
        LeanplumInternal.setHasStarted(false)
    }

    override fun track(event: Event) {
        val leanplumExtras = event.extras
            ?.map { (key, value) -> key.toString() to value }
            ?.toMap()

        event.name?.also {
            Leanplum.track(it, leanplumExtras)
        }
    }

    override fun shouldTrack(event: Event): Boolean {
        return Settings.getInstance(application).isTelemetryEnabled &&
                token.type != Token.Type.Invalid && !event.name.isNullOrEmpty()
    }

    companion object {
        private const val LOGTAG = "LeanplumMetricsService"

        private val LeanplumId: String
            @Suppress("USELESS_ELVIS")
            // Debug builds have a null (nullable) LEANPLUM_ID
            get() = BuildConfig.LEANPLUM_ID ?: ""
        private val LeanplumToken: String
            @Suppress("USELESS_ELVIS")
            // Debug builds have a null (nullable) LEANPLUM_TOKEN
            get() = BuildConfig.LEANPLUM_TOKEN ?: ""
    }
}
