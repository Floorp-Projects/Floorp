/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components.metrics

import org.mozilla.fenix.BuildConfig

sealed class Event {
    object AddBookmark : Event()
    object RemoveBookmark : Event()
    object OpenedBookmark : Event()


    data class OpenedApp(val source: Source) : Event() {
        enum class Source { APP_ICON, CUSTOM_TAB }
        override val extras: Map<String, String>?
            get() = hashMapOf("source" to source.name)
    }

    object OpenedAppFirstRun : Event()
    object InteractWithSearchURLArea : Event()
    object SavedLoginandPassword : Event()
    object FXANewSignup : Event()
    object UserSignedInToFxA : Event()
    object UserDownloadedFocus : Event()
    object UserDownloadedLockbox : Event()
    object UserDownloadedFennec : Event()
    object TrackingProtectionSettingsChanged : Event()
    object FXASyncedNewDevice : Event()
    object DismissedOnboarding : Event()
    object Uninstall : Event()
    object OpenNewNormalTab : Event()
    object OpenNewPrivateTab : Event()
    object ShareStarted : Event()
    object ShareCanceled : Event()
    object ShareCompleted : Event()
    object ClosePrivateTabs : Event()
    object ClearedPrivateData : Event()
    object OpenedLoginManager : Event()
    object OpenedMailtoLink : Event()
    object DownloadMediaSavedImage : Event()
    object UserUsedReaderView : Event()
    object UserDownloadedPocket : Event()
    object UserDownloadedSend : Event()
    object OpenedPocketStory : Event()
    object DarkModeEnabled : Event()

    // Interaction Events
    data class SearchBarTapped(val source: Source) : Event() {
        enum class Source { HOME, BROWSER }
        override val extras: Map<String, String>?
            get() = mapOf("source" to source.name)
    }

    data class EnteredUrl(val autoCompleted: Boolean) : Event() {
        override val extras: Map<String, String>?
            get() = mapOf("autocomplete" to autoCompleted.toString())
    }

    data class PerformedSearch(val fromSearchSuggestion: Boolean) : Event() {
        override val extras: Map<String, String>?
            get() = mapOf("search_suggestion" to fromSearchSuggestion.toString())
    }

    open val extras: Map<String, String>?
        get() = null
}

interface MetricsService {
    fun start()
    fun track(event: Event)
    fun shouldTrack(event: Event): Boolean
}

class Metrics(private val services: List<MetricsService>, private val isTelemetryEnabled: () -> Boolean) {
    private var initialized = false

    fun start() {
        if (BuildConfig.TELEMETRY && !isTelemetryEnabled.invoke() || initialized) { return }

        services.forEach { it.start() }
        initialized = true
    }

    fun track(event: Event) {
        if (BuildConfig.TELEMETRY && !isTelemetryEnabled.invoke() && !initialized) { return }

        services
            .filter { it.shouldTrack(event) }
            .forEach { it.track(event) }
    }
}
