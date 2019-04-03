/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components.metrics

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import org.mozilla.fenix.BuildConfig

sealed class Event {
    object AddBookmark : Event()
    object RemoveBookmark : Event()
    object OpenedBookmark : Event()

    data class OpenedApp(val source: Source) : Event() {
        enum class Source { APP_ICON, LINK, CUSTOM_TAB }
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
    object SearchShortcutMenuOpened : Event()
    object SearchShortcutMenuClosed : Event()

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

    data class PerformedSearch(val fromSearchSuggestion: Boolean, val fromSearchShortcut: Boolean) : Event() {
        override val extras: Map<String, String>?
            get() = mapOf("search_suggestion" to fromSearchSuggestion.toString(),
                "search_shortcut" to fromSearchShortcut.toString())
    }

    // Track only built-in engine selection. Do not track user-added engines!
    data class SearchShortcutSelected(val engine: String) : Event() {
        override val extras: Map<String, String>?
            get() = mapOf("engine" to engine)
    }

    object FindInPageOpened : Event()
    object FindInPageClosed : Event()
    object FindInPageNext : Event()
    object FindInPagePrevious : Event()
    object FindInPageSearchCommitted : Event()

    class ContextMenuItemTapped private constructor(val item: String) : Event() {
        override val extras: Map<String, String>?
            get() = mapOf("named" to item)

        companion object {
            fun create(context_item: String) = allowList[context_item]?.let { ContextMenuItemTapped(it) }

            private val allowList = mapOf(
                "mozac.feature.contextmenu.open_in_new_tab" to "open_in_new_tab",
                "mozac.feature.contextmenu.open_in_private_tab" to "open_in_private_tab",
                "mozac.feature.contextmenu.open_image_in_new_tab" to "open_image_in_new_tab",
                "mozac.feature.contextmenu.save_image" to "save_image",
                "mozac.feature.contextmenu.share_link" to "share_link",
                "mozac.feature.contextmenu.copy_link" to "copy_link",
                "mozac.feature.contextmenu.copy_image_location" to "copy_image_location"
            )
        }
    }

    open val extras: Map<String, String>?
        get() = null
}

private fun Fact.toEvent(): Event? = when (Pair(component, item)) {
    Pair(Component.FEATURE_FINDINPAGE, "previous") -> Event.FindInPagePrevious
    Pair(Component.FEATURE_FINDINPAGE, "next") -> Event.FindInPageNext
    Pair(Component.FEATURE_FINDINPAGE, "close") -> Event.FindInPageClosed
    Pair(Component.FEATURE_FINDINPAGE, "input") -> Event.FindInPageSearchCommitted
    Pair(Component.FEATURE_CONTEXTMENU, "item") -> {
        metadata?.get("item")?.let { Event.ContextMenuItemTapped.create(it.toString()) }
    }

    else -> null
}

interface MetricsService {
    fun start()
    fun track(event: Event)
    fun shouldTrack(event: Event): Boolean
}

class Metrics(private val services: List<MetricsService>, private val isTelemetryEnabled: () -> Boolean) {
    private var initialized = false

    init {
        Facts.registerProcessor(object : FactProcessor {
            override fun process(fact: Fact) {
                fact.toEvent()?.also {
                    track(it)
                }
            }
        })
    }

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
