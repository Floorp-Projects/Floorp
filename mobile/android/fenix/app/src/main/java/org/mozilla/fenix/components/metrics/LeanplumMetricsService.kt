/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.app.Application
import android.content.Context.MODE_PRIVATE
import android.util.Log
import androidx.annotation.VisibleForTesting
import com.leanplum.Leanplum
import com.leanplum.LeanplumActivityHelper
import com.leanplum.annotations.Parser
import com.leanplum.internal.LeanplumInternal
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.support.locale.LocaleManager
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.components.metrics.MozillaProductDetector.MozillaProducts
import org.mozilla.fenix.ext.settings
import java.util.Locale
import java.util.MissingResourceException
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
        is Event.ClearedPrivateData -> "E_Cleared_Private_Data"
        is Event.DismissedOnboarding -> "E_Dismissed_Onboarding"
        is Event.FennecToFenixMigrated -> "E_Fennec_To_Fenix_Migrated"
        is Event.AddonInstalled -> "E_Addon_Installed"
        is Event.SearchWidgetInstalled -> "E_Search_Widget_Added"
        is Event.ChangedToDefaultBrowser -> "E_Changed_Default_To_Fenix"
        is Event.TrackingProtectionSettingChanged -> "E_Changed_ETP"

        // Do not track other events in Leanplum
        else -> null
    }

class LeanplumMetricsService(
    private val application: Application,
    private val deviceIdGenerator: () -> String = { randomUUID().toString() }
) : MetricsService {
    val scope = CoroutineScope(Dispatchers.IO)
    var leanplumJob: Job? = null

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

    override val type = MetricServiceType.Marketing
    private val token = Token(LeanplumId, LeanplumToken)

    private val preferences = application.getSharedPreferences(PREFERENCE_NAME, MODE_PRIVATE)

    @VisibleForTesting
    internal val deviceId by lazy {
        var deviceId = preferences.getString(DEVICE_ID_KEY, null)

        if (deviceId == null) {
            deviceId = deviceIdGenerator.invoke()
            preferences.edit().putString(DEVICE_ID_KEY, deviceId).apply()
        }

        deviceId
    }

    override fun start() {

        if (!application.settings().isMarketingTelemetryEnabled) return

        Leanplum.setIsTestModeEnabled(false)
        Leanplum.setApplicationContext(application)
        Leanplum.setDeviceId(deviceId)
        Parser.parseVariables(application)

        leanplumJob = scope.launch {

            val applicationSetLocale = LocaleManager.getCurrentLocale(application)
            val currentLocale = applicationSetLocale ?: Locale.getDefault()
            val languageCode =
                currentLocale.iso3LanguageOrNull
                ?: currentLocale.language.let {
                    if (it.isNotBlank()) {
                        it
                    } else {
                        currentLocale.toString()
                    }
                }

            if (!isLeanplumEnabled(languageCode)) {
                Log.i(LOGTAG, "Leanplum is not available for this locale: $languageCode")
                return@launch
            }

            when (token.type) {
                Token.Type.Production -> Leanplum.setAppIdForProductionMode(token.id, token.token)
                Token.Type.Development -> Leanplum.setAppIdForDevelopmentMode(token.id, token.token)
                Token.Type.Invalid -> {
                    Log.i(LOGTAG, "Invalid or missing Leanplum token")
                    return@launch
                }
            }

            LeanplumActivityHelper.enableLifecycleCallbacks(application)

            val installedApps = MozillaProductDetector.getInstalledMozillaProducts(application)

            val trackingProtection = application.settings().run {
                when {
                    !shouldUseTrackingProtection -> "none"
                    useStandardTrackingProtection -> "standard"
                    useStrictTrackingProtection -> "strict"
                    else -> "custom"
                }
            }

            Leanplum.start(application, hashMapOf(
                "default_browser" to MozillaProductDetector.getMozillaBrowserDefault(application).orEmpty(),
                "fennec_installed" to installedApps.contains(MozillaProducts.FIREFOX.productName),
                "focus_installed" to installedApps.contains(MozillaProducts.FOCUS.productName),
                "klar_installed" to installedApps.contains(MozillaProducts.KLAR.productName),
                "fxa_signed_in" to application.settings().fxaSignedIn,
                "fxa_has_synced_items" to application.settings().fxaHasSyncedItems,
                "search_widget_installed" to application.settings().searchWidgetInstalled,
                "tracking_protection_enabled" to application.settings().shouldUseTrackingProtection,
                "tracking_protection_setting" to trackingProtection,
                "fenix" to true
            ))

            withContext(Main) {
                LeanplumInternal.setCalledStart(true)
                LeanplumInternal.setHasStarted(true)
                LeanplumInternal.setStartedInBackground(true)
            }
        }
    }

    override fun stop() {
        if (application.settings().isMarketingTelemetryEnabled) return
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
        leanplumJob?.cancel()
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
        return application.settings().isTelemetryEnabled &&
                token.type != Token.Type.Invalid && !event.name.isNullOrEmpty()
    }

    private fun isLeanplumEnabled(locale: String): Boolean {
        return LEANPLUM_ENABLED_LOCALES.contains(locale)
    }

    private val Locale.iso3LanguageOrNull: String?
        get() =
            try {
                this.isO3Language
            } catch (_: MissingResourceException) { null }

    companion object {
        private const val LOGTAG = "LeanplumMetricsService"

        private val LeanplumId: String
            // Debug builds have a null (nullable) LEANPLUM_ID
            get() = BuildConfig.LEANPLUM_ID.orEmpty()
        private val LeanplumToken: String
            // Debug builds have a null (nullable) LEANPLUM_TOKEN
            get() = BuildConfig.LEANPLUM_TOKEN.orEmpty()
        // Leanplum needs to be enabled for the following locales.
        // Irrespective of the actual device location.
        private val LEANPLUM_ENABLED_LOCALES = setOf(
            "eng", // English
            "zho", // Chinese
            "deu", // German
            "fra", // French
            "ita", // Italian
            "ind", // Indonesian
            "por", // Portuguese
            "spa", // Spanish; Castilian
            "pol", // Polish
            "rus", // Russian
            "hin", // Hindi
            "per", // Persian
            "fas", // Persian
            "ara", // Arabic
            "jpn" // Japanese
        )

        private val PREFERENCE_NAME = "LEANPLUM_PREFERENCES"
        private val DEVICE_ID_KEY = "LP_DEVICE_ID"
    }
}
