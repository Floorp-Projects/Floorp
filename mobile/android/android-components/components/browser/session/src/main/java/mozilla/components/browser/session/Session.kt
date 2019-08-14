/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import android.graphics.Bitmap
import mozilla.components.browser.session.engine.EngineSessionHolder
import mozilla.components.browser.session.engine.request.LoadRequestMetadata
import mozilla.components.browser.session.engine.request.LoadRequestOption
import mozilla.components.browser.session.ext.syncDispatch
import mozilla.components.browser.session.ext.toSecurityInfoState
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.browser.state.action.ContentAction.RemoveIconAction
import mozilla.components.browser.state.action.ContentAction.RemoveThumbnailAction
import mozilla.components.browser.state.action.ContentAction.UpdateIconAction
import mozilla.components.browser.state.action.ContentAction.UpdateLoadingStateAction
import mozilla.components.browser.state.action.ContentAction.UpdateProgressAction
import mozilla.components.browser.state.action.ContentAction.UpdateSecurityInfo
import mozilla.components.browser.state.action.ContentAction.UpdateSearchTermsAction
import mozilla.components.browser.state.action.ContentAction.UpdateThumbnailAction
import mozilla.components.browser.state.action.ContentAction.UpdateTitleAction
import mozilla.components.browser.state.action.ContentAction.UpdateUrlAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.util.UUID
import kotlin.properties.Delegates

/**
 * Value type that represents the state of a browser session. Changes can be observed.
 */
@Suppress("TooManyFunctions")
class Session(
    initialUrl: String,
    val private: Boolean = false,
    val source: Source = Source.NONE,
    val id: String = UUID.randomUUID().toString(),
    delegate: Observable<Observer> = ObserverRegistry()
) : Observable<Session.Observer> by delegate {
    /**
     * Holder for keeping a reference to an engine session and its observer to update this session
     * object.
     */
    internal val engineSessionHolder = EngineSessionHolder()

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
        fun onNavigationStateChanged(session: Session, canGoBack: Boolean, canGoForward: Boolean) = Unit
        fun onLoadRequest(
            session: Session,
            url: String,
            triggeredByRedirect: Boolean,
            triggeredByWebContent: Boolean
        ) = Unit
        fun onSearch(session: Session, searchTerms: String) = Unit
        fun onSecurityChanged(session: Session, securityInfo: SecurityInfo) = Unit
        fun onCustomTabConfigChanged(session: Session, customTabConfig: CustomTabConfig?) = Unit
        fun onWebAppManifestChanged(session: Session, manifest: WebAppManifest?) = Unit
        fun onDownload(session: Session, download: Download): Boolean = false
        fun onTrackerBlockingEnabledChanged(session: Session, blockingEnabled: Boolean) = Unit
        fun onTrackerBlocked(session: Session, tracker: Tracker, all: List<Tracker>) = Unit
        fun onTrackerLoaded(session: Session, tracker: Tracker, all: List<Tracker>) = Unit
        fun onLongPress(session: Session, hitResult: HitResult): Boolean = false
        fun onFindResult(session: Session, result: FindResult) = Unit
        fun onDesktopModeChanged(session: Session, enabled: Boolean) = Unit
        fun onFullScreenChanged(session: Session, enabled: Boolean) = Unit
        fun onThumbnailChanged(session: Session, bitmap: Bitmap?) = Unit
        fun onContentPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean = false
        fun onAppPermissionRequested(session: Session, permissionRequest: PermissionRequest): Boolean = false
        fun onPromptRequested(session: Session, promptRequest: PromptRequest): Boolean = false
        fun onOpenWindowRequested(session: Session, windowRequest: WindowRequest): Boolean = false
        fun onCloseWindowRequested(session: Session, windowRequest: WindowRequest): Boolean = false
        fun onMediaRemoved(session: Session, media: List<Media>, removed: Media) = Unit
        fun onMediaAdded(session: Session, media: List<Media>, added: Media) = Unit
        fun onCrashStateChanged(session: Session, crashed: Boolean) = Unit
        fun onIconChanged(session: Session, icon: Bitmap?) = Unit
        fun onReaderableStateUpdated(session: Session, readerable: Boolean) = Unit
        fun onReaderModeChanged(session: Session, enabled: Boolean) = Unit
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
     * Represents the origin of a session to describe how and why it was created.
     */
    enum class Source {
        /**
         * Created to handle an ACTION_SEND (share) intent
         */
        ACTION_SEND,

        /**
         * Created to handle an ACTION_VIEW intent
         */
        ACTION_VIEW,

        /**
         * Created to handle a CustomTabs intent
         */
        CUSTOM_TAB,

        /**
         * User interacted with the home screen
         */
        HOME_SCREEN,

        /**
         * User interacted with a menu
         */
        MENU,

        /**
         * User opened a new tab
         */
        NEW_TAB,

        /**
         * Default value and for testing purposes
         */
        NONE,

        /**
         * Default value and for testing purposes
         */
        TEXT_SELECTION,

        /**
         * User entered a URL or search term
         */
        USER_ENTERED
    }

    /**
     * A value type representing a result of a "find in page" operation.
     *
     * @property activeMatchOrdinal the zero-based ordinal of the currently selected match.
     * @property numberOfMatches the match count
     * @property isDoneCounting true if the find operation has completed, otherwise false.
     */
    data class FindResult(val activeMatchOrdinal: Int, val numberOfMatches: Int, val isDoneCounting: Boolean)

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
     * Navigation state, true if there's an history item to go back to, otherwise false.
     */
    var canGoBack: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onNavigationStateChanged(this@Session, new, canGoForward) }
    }

    /**
     * Navigation state, true if there's an history item to go forward to, otherwise false.
     */
    var canGoForward: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onNavigationStateChanged(this@Session, canGoBack, new) }
    }

    /**
     * The currently / last used search terms (or an empty string).
     */
    var searchTerms: String by Delegates.observable("") { _, _, new ->
        notifyObservers { onSearch(this@Session, new) }
        store?.syncDispatch(UpdateSearchTermsAction(id, new))
    }

    /**
     * Set when a load request is received, indicating if the request came from web content, or via a redirect.
     */
    var loadRequestMetadata: LoadRequestMetadata by Delegates.observable(LoadRequestMetadata.blank) { _, _, new ->
        notifyObservers {
            onLoadRequest(
                this@Session,
                new.url,
                new.isSet(LoadRequestOption.REDIRECT),
                new.isSet(LoadRequestOption.WEB_CONTENT)
            )
        }
    }

    /**
     * Security information indicating whether or not the current session is
     * for a secure URL, as well as the host and SSL certificate authority, if applicable.
     */
    var securityInfo: SecurityInfo by Delegates.observable(SecurityInfo()) { _, old, new ->
        notifyObservers(old, new) { onSecurityChanged(this@Session, new) }
        store?.syncDispatch(UpdateSecurityInfo(id, new.toSecurityInfoState()))
    }

    /**
     * Configuration data in case this session is used for a Custom Tab.
     */
    var customTabConfig: CustomTabConfig? by Delegates.observable<CustomTabConfig?>(null) { _, _, new ->
        notifyObservers { onCustomTabConfigChanged(this@Session, new) }
    }

    /**
     * The Web App Manifest for the currently visited page (or null).
     */
    var webAppManifest: WebAppManifest? by Delegates.observable<WebAppManifest?>(null) { _, _, new ->
        notifyObservers { onWebAppManifestChanged(this@Session, new) }
    }

    /**
     * Last download request if it wasn't consumed by at least one observer.
     */
    var download: Consumable<Download> by Delegates.vetoable(Consumable.empty()) { _, _, download ->
        val consumers = wrapConsumers<Download> { onDownload(this@Session, it) }
        !download.consumeBy(consumers)
    }

    /**
     * List of [Media] on the currently visited page.
     */
    var media: List<Media> by Delegates.observable(emptyList()) { _, old, new ->
        if (old.size > new.size) {
            val removed = old - new
            require(removed.size == 1) { "Expected only one item to be removed, but was ${removed.size}" }
            notifyObservers {
                onMediaRemoved(this@Session, new, removed[0])
            }
        } else if (new.size > old.size) {
            val added = new - old
            require(added.size == 1) { "Expected only one item to be added, but was ${added.size}" }
            notifyObservers {
                onMediaAdded(this@Session, new, added[0])
            }
        }
    }

    /**
     * Tracker blocking state, true if blocking trackers is enabled, otherwise false.
     */
    var trackerBlockingEnabled: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onTrackerBlockingEnabledChanged(this@Session, new) }
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
    }

    /**
     * List of results of that latest "find in page" operation.
     */
    var findResults: List<FindResult> by Delegates.observable(emptyList()) { _, old, new ->
        notifyObservers(old, new) {
            if (new.isNotEmpty()) {
                onFindResult(this@Session, new.last())
            }
        }
    }

    /**
     * The target of the latest long click operation.
     */
    var hitResult: Consumable<HitResult> by Delegates.vetoable(Consumable.empty()) { _, _, result ->
        val consumers = wrapConsumers<HitResult> { onLongPress(this@Session, it) }
        !result.consumeBy(consumers)
    }

    /**
     * The target of the latest thumbnail.
     */
    var thumbnail: Bitmap? by Delegates.observable<Bitmap?>(null) { _, _, new ->
        notifyObservers { onThumbnailChanged(this@Session, new) }

        val action = if (new != null) UpdateThumbnailAction(id, new) else RemoveThumbnailAction(id)
        store?.syncDispatch(action)
    }

    /**
     * Desktop Mode state, true if the desktop mode is requested, otherwise false.
     */
    var desktopMode: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onDesktopModeChanged(this@Session, new) }
    }

    /**
     * Exits fullscreen mode if it's in that state.
     */
    var fullScreenMode: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onFullScreenChanged(this@Session, new) }
    }

    /**
     * An icon for the currently visible page.
     */
    var icon: Bitmap? by Delegates.observable<Bitmap?>(null) { _, old, new ->
        if (notifyObservers(old, new) { onIconChanged(this@Session, new) }) {
            val action = if (new != null) UpdateIconAction(id, new) else RemoveIconAction(id)
            store?.syncDispatch(action)
        }
    }

    /**
     * [Consumable] permission request from web content. A [PermissionRequest]
     * must be consumed i.e. either [PermissionRequest.grant] or
     * [PermissionRequest.reject] must be called. A content permission request
     * can also be cancelled, which will result in a new empty [Consumable].
     */
    var contentPermissionRequest: Consumable<PermissionRequest> by Delegates.vetoable(Consumable.empty()) {
        _, _, request ->
            val consumers = wrapConsumers<PermissionRequest> { onContentPermissionRequested(this@Session, it) }
            !request.consumeBy(consumers)
    }

    /**
     * [Consumable] permission request for the app. A [PermissionRequest]
     * must be consumed i.e. either [PermissionRequest.grant] or
     * [PermissionRequest.reject] must be called.
     */
    var appPermissionRequest: Consumable<PermissionRequest> by Delegates.vetoable(Consumable.empty()) {
        _, _, request ->
            val consumers = wrapConsumers<PermissionRequest> { onAppPermissionRequested(this@Session, it) }
            !request.consumeBy(consumers)
    }

    /**
     * [Consumable] State for a prompt request from web content.
     */
    var promptRequest: Consumable<PromptRequest> by Delegates.vetoable(Consumable.empty()) {
        _, _, request ->
            val consumers = wrapConsumers<PromptRequest> { onPromptRequested(this@Session, it) }
            !request.consumeBy(consumers)
    }

    /**
     * [Consumable] request to open/create a window.
     */
    var openWindowRequest: Consumable<WindowRequest> by Delegates.vetoable(Consumable.empty()) { _, _, request ->
        val consumers = wrapConsumers<WindowRequest> { onOpenWindowRequested(this@Session, it) }
        !request.consumeBy(consumers)
    }

    /**
     * [Consumable] request to close a window.
     */
    var closeWindowRequest: Consumable<WindowRequest> by Delegates.vetoable(Consumable.empty()) { _, _, request ->
        val consumers = wrapConsumers<WindowRequest> { onCloseWindowRequested(this@Session, it) }
        !request.consumeBy(consumers)
    }

    /**
     * Whether this [Session] has crashed.
     *
     * In conjunction with a `concept-engine` implementation that uses a multi-process architecture, single sessions
     * can crash without crashing the whole app.
     *
     * A crashed session may still be operational (since the underlying engine implementation has recovered its content
     * process), but further action may be needed to restore the last state before the session has crashed (if desired).
     */
    var crashed: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onCrashStateChanged(this@Session, new) }
    }

    /**
     * Readerable state, whether or not the current page can be shown in a reader view.
     */
    var readerable: Boolean by Delegates.observable(false) { _, _, new ->
        notifyObservers { onReaderableStateUpdated(this@Session, new) }
    }

    /**
     * Reader mode state, whether or not reader view is enabled, otherwise false.
     */
    var readerMode: Boolean by Delegates.observable(false) { _, old, new ->
        notifyObservers(old, new) { onReaderModeChanged(this@Session, new) }
    }

    /**
     * List of recording devices (e.g. camera or microphone) currently in use by web content.
     */
    var recordingDevices: List<RecordingDevice> by Delegates.observable(emptyList()) { _, old, new ->
        notifyObservers(old, new) { onRecordingDevicesChanged(this@Session, new) }
    }

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
