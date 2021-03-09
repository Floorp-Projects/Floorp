/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.browser.session

import android.graphics.Bitmap
import mozilla.components.browser.session.ext.syncDispatch
import mozilla.components.browser.session.ext.toSecurityInfoState
import mozilla.components.browser.state.action.ContentAction.RemoveWebAppManifestAction
import mozilla.components.browser.state.action.ContentAction.UpdateLoadingStateAction
import mozilla.components.browser.state.action.ContentAction.UpdateProgressAction
import mozilla.components.browser.state.action.ContentAction.UpdateSecurityInfoAction
import mozilla.components.browser.state.action.ContentAction.UpdateTitleAction
import mozilla.components.browser.state.action.ContentAction.UpdateUrlAction
import mozilla.components.browser.state.action.ContentAction.UpdateWebAppManifestAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.support.base.observer.DeprecatedObservable
import mozilla.components.support.base.observer.DeprecatedObserverRegistry
import java.util.UUID
import kotlin.properties.Delegates

/**
 * Value type that represents the state of a browser session. Changes can be observed.
 */
@Suppress("TooManyFunctions", "LongParameterList")
class Session(
    initialUrl: String,
    val private: Boolean = false,
    val source: SessionState.Source = SessionState.Source.NONE,
    val id: String = UUID.randomUUID().toString(),
    val contextId: String? = null,
    delegate: DeprecatedObserverRegistry<Observer> = DeprecatedObserverRegistry()
) : DeprecatedObservable<Session.Observer> by delegate {
    // For migration purposes every `Session` has a reference to the `BrowserStore` (if used) in order to dispatch
    // actions to it when the `Session` changes.
    internal var store: BrowserStore? = null

    /**
     * Id of parent session, usually refer to the session which created this one. The clue to indicate if this session
     * is terminated, which target we should go back.
     */
    internal var parentId: String? = null

    /**
     * Interface to be implemented by classes that want to observe a session.
     */
    interface Observer {
        fun onUrlChanged(session: Session, url: String) = Unit
        fun onTitleChanged(session: Session, title: String) = Unit
        fun onProgress(session: Session, progress: Int) = Unit
        fun onLoadingStateChanged(session: Session, loading: Boolean) = Unit
        fun onLoadRequest(
            session: Session,
            url: String,
            triggeredByRedirect: Boolean,
            triggeredByWebContent: Boolean
        ) = Unit
        fun onSecurityChanged(session: Session, securityInfo: SecurityInfo) = Unit
        fun onCustomTabConfigChanged(session: Session, customTabConfig: CustomTabConfig?) = Unit
        fun onWebAppManifestChanged(session: Session, manifest: WebAppManifest?) = Unit
        fun onTrackerBlockingEnabledChanged(session: Session, blockingEnabled: Boolean) = Unit
        fun onTrackerBlocked(session: Session, tracker: Tracker, all: List<Tracker>) = Unit
        fun onTrackerLoaded(session: Session, tracker: Tracker, all: List<Tracker>) = Unit
        fun onContentPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean = false
        fun onRecordingDevicesChanged(session: Session, devices: List<RecordingDevice>) = Unit
    }

    /**
     * A value type holding security information for a Session.
     *
     * @property secure true if the session is currently pointed to a URL with
     * a valid SSL certificate, otherwise false.
     * @property host domain for which the SSL certificate was issued.
     * @property issuer name of the certificate authority who issued the SSL certificate.
     */
    data class SecurityInfo(val secure: Boolean = false, val host: String = "", val issuer: String = "")

    /**
     * The currently loading or loaded URL.
     */
    var url: String by Delegates.observable(initialUrl) { _, old, new ->
        if (notifyObservers(old, new) { onUrlChanged(this@Session, new) }) {
            store?.syncDispatch(UpdateUrlAction(id, new))
        }
    }

    /**
     * The title of the currently displayed website changed.
     */
    var title: String by Delegates.observable("") { _, old, new ->
        if (notifyObservers(old, new) { onTitleChanged(this@Session, new) }) {
            store?.syncDispatch(UpdateTitleAction(id, new))
        }
    }

    /**
     * The progress loading the current URL.
     */
    var progress: Int by Delegates.observable(0) { _, old, new ->
        if (notifyObservers(old, new) { onProgress(this@Session, new) }) {
            store?.syncDispatch(UpdateProgressAction(id, new))
        }
    }

    /**
     * Loading state, true if this session's url is currently loading, otherwise false.
     */
    var loading: Boolean by Delegates.observable(false) { _, old, new ->
        if (notifyObservers(old, new) { onLoadingStateChanged(this@Session, new) }) {
            store?.syncDispatch(UpdateLoadingStateAction(id, new))
        }
    }

    /**
     * Security information indicating whether or not the current session is
     * for a secure URL, as well as the host and SSL certificate authority, if applicable.
     */
    var securityInfo: SecurityInfo by Delegates.observable(SecurityInfo()) { _, old, new ->
        notifyObservers(old, new) { onSecurityChanged(this@Session, new) }
        store?.syncDispatch(UpdateSecurityInfoAction(id, new.toSecurityInfoState()))
    }

    /**
     * Configuration data in case this session is used for a Custom Tab.
     */
    var customTabConfig: CustomTabConfig? by Delegates.observable<CustomTabConfig?>(null) { _, old, new ->
        notifyObservers { onCustomTabConfigChanged(this@Session, new) }

        // The custom tab config is set to null when we're migrating custom
        // tabs to regular tabs, so we have to dispatch the corresponding
        // browser actions to keep the store in sync.
        if (old != new && new == null) {
            store?.syncDispatch(
                CustomTabListAction.TurnCustomTabIntoNormalTabAction(id)
            )
        }
    }

    /**
     * The Web App Manifest for the currently visited page (or null).
     */
    var webAppManifest: WebAppManifest? by Delegates.observable<WebAppManifest?>(null) { _, old, new ->
        notifyObservers { onWebAppManifestChanged(this@Session, new) }

        if (old != new) {
            val action = if (new != null) {
                UpdateWebAppManifestAction(id, new)
            } else {
                RemoveWebAppManifestAction(id)
            }
            store?.syncDispatch(action)
        }
    }

    /**
     * Tracker blocking state, true if blocking trackers is enabled, otherwise false.
     */
    var trackerBlockingEnabled: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onTrackerBlockingEnabledChanged(this@Session, new) }

        store?.syncDispatch(TrackingProtectionAction.ToggleAction(id, new))
    }

    /**
     * List of [Tracker]s that have been blocked in this session.
     */
    var trackersBlocked: List<Tracker> by Delegates.observable(emptyList()) { _, old, new ->
        notifyObservers(old, new) {
            if (new.isNotEmpty()) {
                onTrackerBlocked(this@Session, new.last(), new)
            }
        }

        if (new.isEmpty()) {
            // From `EngineObserver` we can assume that this means the trackers have been cleared.
            // The `ClearTrackersAction` will also clear the loaded trackers list. That is always
            // the case when this list is cleared from `EngineObserver`. For the sake of migrating
            // to browser-state we assume that no other code changes the tracking properties.
            store?.syncDispatch(TrackingProtectionAction.ClearTrackersAction(id))
        } else {
            // `EngineObserver` always adds new trackers to the end of the list. So we just dispatch
            // an action for the last item in the list.
            store?.syncDispatch(TrackingProtectionAction.TrackerBlockedAction(id, new.last()))
        }
    }

    /**
     * List of [Tracker]s that could be blocked but have been loaded in this session.
     */
    var trackersLoaded: List<Tracker> by Delegates.observable(emptyList()) { _, old, new ->
        notifyObservers(old, new) {
            if (new.isNotEmpty()) {
                onTrackerLoaded(this@Session, new.last(), new)
            }
        }

        if (new.isNotEmpty()) {
            // The empty case is already handled by the `trackersBlocked` property since both
            // properties are always cleared together by `EngineObserver`.
            // `EngineObserver` always adds new trackers to the end of the list. So we just dispatch
            // an action for the last item in the list.
            store?.syncDispatch(TrackingProtectionAction.TrackerLoadedAction(id, new.last()))
        }
    }

    /**
     * An icon for the currently visible page.
     */
    val icon: Bitmap?
        // This is a workaround until all callers are migrated to use browser-state. Until then
        // we try to lookup the icon from an attached BrowserStore if possible.
        get() = store?.state?.findTabOrCustomTab(id)?.content?.icon

    /**
     * Returns whether or not this session is used for a Custom Tab.
     */
    fun isCustomTabSession() = customTabConfig != null

    /**
     * Helper method to notify observers only if a value changed.
     *
     * @param old the previous value of a session property
     * @param new the current (new) value of a session property
     *
     * @return true if the observers where notified (the values changed), otherwise false.
     */
    private fun notifyObservers(old: Any?, new: Any?, block: Observer.() -> Unit): Boolean {
        return if (old != new) {
            notifyObservers(block)
            true
        } else {
            false
        }
    }

    /**
     * Returns true if this [Session] has a parent [Session].
     *
     * A [Session] can have a parent [Session] if one was provided when calling [SessionManager.add]. The parent
     * [Session] is usually the [Session] the new [Session] was opened from - like opening a new tab from a link
     * context menu ("Open in new tab").
     */
    val hasParentSession: Boolean
        get() = parentId != null

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as Session
        if (id != other.id) return false
        return true
    }

    override fun hashCode(): Int {
        return id.hashCode()
    }

    override fun toString(): String {
        return "Session($id, $url)"
    }
}
