/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components.metrics

sealed class Event {
    object AddBookmark : Event()
    object RemoveBookmark : Event()
    object OpenedBookmark : Event()
    object OpenedApp : Event()
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

    val extras: Map<String, Any>?
        get() = null
}

interface MetricsService {
    fun start()
    fun track(event: Event)
    fun shouldTrack(event: Event): Boolean
}

class Metrics(private val services: List<MetricsService>) {
    fun start() {
        services.forEach { it.start() }
    }

    fun track(event: Event) {
        services
            .filter { it.shouldTrack(event) }
            .forEach { it.track(event) }
    }
}
