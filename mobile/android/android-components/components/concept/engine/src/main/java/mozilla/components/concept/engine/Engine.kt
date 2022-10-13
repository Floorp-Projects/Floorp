/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.content.Context
import android.os.Parcelable
import android.util.AttributeSet
import android.util.JsonReader
import androidx.annotation.MainThread
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.activity.ActivityDelegate
import mozilla.components.concept.engine.activity.OrientationDelegate
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.concept.engine.serviceworker.ServiceWorkerDelegate
import mozilla.components.concept.engine.utils.EngineVersion
import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.concept.engine.webnotifications.WebNotificationDelegate
import mozilla.components.concept.engine.webpush.WebPushDelegate
import mozilla.components.concept.engine.webpush.WebPushHandler
import org.json.JSONObject

/**
 * Entry point for interacting with the engine implementation.
 */
interface Engine : WebExtensionRuntime, DataCleanable {

    /**
     * Describes a combination of browsing data types stored by the engine.
     */
    class BrowsingData internal constructor(val types: Int) {
        companion object {
            const val COOKIES: Int = 1 shl 0
            const val NETWORK_CACHE: Int = 1 shl 1
            const val IMAGE_CACHE: Int = 1 shl 2
            const val DOM_STORAGES: Int = 1 shl 4
            const val AUTH_SESSIONS: Int = 1 shl 5
            const val PERMISSIONS: Int = 1 shl 6
            const val ALL_CACHES: Int = NETWORK_CACHE + IMAGE_CACHE
            const val ALL_SITE_SETTINGS: Int = (1 shl 7) + PERMISSIONS
            const val ALL_SITE_DATA: Int = (1 shl 8) + COOKIES + DOM_STORAGES + ALL_CACHES + ALL_SITE_SETTINGS
            const val ALL: Int = 1 shl 9

            fun allCaches() = BrowsingData(ALL_CACHES)
            fun allSiteSettings() = BrowsingData(ALL_SITE_SETTINGS)
            fun allSiteData() = BrowsingData(ALL_SITE_DATA)
            fun all() = BrowsingData(ALL)
            fun select(vararg types: Int) = BrowsingData(types.sum())
        }

        fun contains(type: Int) = (types and type) != 0 || types == ALL

        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is BrowsingData) return false
            if (types != other.types) return false
            return true
        }

        override fun hashCode() = types
    }

    /**
     * HTTPS-Only mode: Connections will be upgraded to HTTPS.
     */
    enum class HttpsOnlyMode {
        /**
         * HTTPS-Only Mode disabled: Allow all insecure connections.
         */
        DISABLED,

        /**
         * HTTPS-Only Mode enabled only in private tabs: Allow insecure connections in normal
         * browsing, but only HTTPS in private browsing.
         */
        ENABLED_PRIVATE_ONLY,

        /**
         * HTTPS-Only Mode enabled: Only allow HTTPS connections.
         */
        ENABLED,
    }

    /**
     * Makes sure all required engine initialization logic is executed. The
     * details are specific to individual implementations, but the following must be true:
     *
     * - The engine must be operational after this method was called successfully
     * - Calling this method on an engine that is already initialized has no effect
     */
    @MainThread
    fun warmUp() = Unit

    /**
     * Creates a new view for rendering web content.
     *
     * @param context an application context
     * @param attrs optional set of attributes
     *
     * @return new newly created [EngineView].
     */
    fun createView(context: Context, attrs: AttributeSet? = null): EngineView

    /**
     * Creates a new engine session. If [speculativeCreateSession] is supported this
     * method returns the prepared [EngineSession] if it is still applicable i.e.
     * the parameter(s) ([private]) are equal.
     *
     * @param private whether or not this session should use private mode.
     * @param contextId the session context ID for this session.
     *
     * @return the newly created [EngineSession].
     */
    @MainThread
    fun createSession(private: Boolean = false, contextId: String? = null): EngineSession

    /**
     * Create a new [EngineSessionState] instance from the serialized JSON representation.
     */
    fun createSessionState(json: JSONObject): EngineSessionState

    /**
     * Creates a new [EngineSessionState] instances from the serialized JSON representation.
     */
    fun createSessionStateFrom(reader: JsonReader): EngineSessionState

    /**
     * Returns the name of this engine. The returned string might be used
     * in filenames and must therefore only contain valid filename
     * characters.
     *
     * @return the engine name as specified by concrete implementations.
     */
    fun name(): String

    /**
     * Opens a speculative connection to the host of [url].
     *
     * This is useful if an app thinks it may be making a request to that host in the near future. If no request
     * is made, the connection will be cleaned up after an unspecified.
     *
     * Not all [Engine] implementations may actually implement this.
     */
    fun speculativeConnect(url: String)

    /**
     * Informs the engine that an [EngineSession] is likely to be requested soon
     * via [createSession]. This is useful in case creating an engine session is
     * costly and an application wants to decide when the session should be created
     * without having to manage the session itself i.e. when it may or may not
     * need it.
     *
     * @param private whether or not the session should use private mode.
     * @param contextId the session context ID for the session.
     */
    @MainThread
    fun speculativeCreateSession(private: Boolean = false, contextId: String? = null) = Unit

    /**
     * Removes and closes a speculative session created by [speculativeCreateSession]. This is
     * useful in case the session should no longer be used e.g. because engine settings have
     * changed.
     */
    @MainThread
    fun clearSpeculativeSession() = Unit

    /**
     * Registers a [WebNotificationDelegate] to be notified of engine events
     * related to web notifications
     *
     * @param webNotificationDelegate callback to be invoked for web notification events.
     */
    fun registerWebNotificationDelegate(
        webNotificationDelegate: WebNotificationDelegate,
    ): Unit = throw UnsupportedOperationException("Web notification support is not available in this engine")

    /**
     * Registers a [WebPushDelegate] to be notified of engine events related to web extensions.
     *
     * @return A [WebPushHandler] to notify the engine with messages and subscriptions when are delivered.
     */
    fun registerWebPushDelegate(
        webPushDelegate: WebPushDelegate,
    ): WebPushHandler = throw UnsupportedOperationException("Web Push support is not available in this engine")

    /**
     * Registers an [ActivityDelegate] to be notified on activity events that are needed by the engine.
     */
    fun registerActivityDelegate(
        activityDelegate: ActivityDelegate,
    ): Unit = throw UnsupportedOperationException("This engine does not have support for an Activity delegate.")

    /**
     * Un-registers the attached [ActivityDelegate] if one was added with [registerActivityDelegate].
     */
    fun unregisterActivityDelegate(): Unit =
        throw UnsupportedOperationException("This engine does not have support for an Activity delegate.")

    /**
     * Registers an [OrientationDelegate] to be notified when a website asked the engine
     * to lock the the app on a certain screen orientation.
     */
    fun registerScreenOrientationDelegate(
        delegate: OrientationDelegate,
    ): Unit = throw UnsupportedOperationException("This engine does not have support for an Activity delegate.")

    /**
     * Un-registers the attached [OrientationDelegate] if one was added with
     * [registerScreenOrientationDelegate].
     */
    fun unregisterScreenOrientationDelegate(): Unit =
        throw UnsupportedOperationException("This engine does not have support for an Activity delegate.")

    /**
     * Registers a [ServiceWorkerDelegate] to be notified of service workers events and requests.
     *
     * @param serviceWorkerDelegate [ServiceWorkerDelegate] responding to all service workers events and requests.
     */
    fun registerServiceWorkerDelegate(
        serviceWorkerDelegate: ServiceWorkerDelegate,
    ): Unit = throw UnsupportedOperationException("Service workers support not available in this engine")

    /**
     * Un-registers the attached [ServiceWorkerDelegate] if one was added with
     * [registerServiceWorkerDelegate].
     */
    fun unregisterServiceWorkerDelegate(): Unit =
        throw UnsupportedOperationException("Service workers support not available in this engine")

    /**
     * Handles user interacting with a web notification.
     *
     * @param webNotification [Parcelable] representing a web notification.
     * If the `Parcelable` is not a web notification this method will be no-op.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification">MDN Notification docs</a>
     */
    fun handleWebNotificationClick(webNotification: Parcelable): Unit =
        throw UnsupportedOperationException("Web notification clicks not yet supported in this engine")

    /**
     * Fetch a list of trackers logged for a given [session] .
     *
     * @param session the session where the trackers were logged.
     * @param onSuccess callback invoked if the data was fetched successfully.
     * @param onError (optional) callback invoked if fetching the data caused an exception.
     */
    fun getTrackersLog(
        session: EngineSession,
        onSuccess: (List<TrackerLog>) -> Unit,
        onError: (Throwable) -> Unit = { },
    ): Unit = onError(
        UnsupportedOperationException(
            "getTrackersLog is not supported by this engine.",
        ),
    )

    /**
     * Provides access to the tracking protection exception list for this engine.
     */
    val trackingProtectionExceptionStore: TrackingProtectionExceptionStorage
        get() = throw UnsupportedOperationException("TrackingProtectionExceptionStorage not supported by this engine.")

    /**
     * Provides access to Firefox Profiler features.
     * See [Profiler] for more information.
     */
    val profiler: Profiler?

    /**
     * Provides access to the settings of this engine.
     */
    val settings: Settings

    /**
     * Returns the version of the engine as [EngineVersion] object.
     */
    val version: EngineVersion
}
