/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.content.Context
import android.util.AttributeSet
import androidx.annotation.MainThread
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.utils.EngineVersion
import mozilla.components.concept.engine.webextension.WebExtension
import org.json.JSONObject
import java.lang.UnsupportedOperationException

/**
 * Entry point for interacting with the engine implementation.
 */
interface Engine {

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
     * Creates a new engine session.
     *
     * @param private whether or not this session should use private mode.
     *
     * @return the newly created [EngineSession].
     */
    fun createSession(private: Boolean = false): EngineSession

    /**
     * Create a new [EngineSessionState] instance from the serialized JSON representation.
     */
    fun createSessionState(json: JSONObject): EngineSessionState

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
     * Installs the provided extension in this engine.
     *
     * @param id the unique ID of the extension.
     * @param url the url pointing to a resources path for locating the extension
     * within the APK file e.g. resource://android/assets/extensions/my_web_ext.
     * @param allowContentMessaging whether or not the web extension is allowed
     * to send messages from content scripts, defaults to true.
     * @param onSuccess (optional) callback invoked if the extension was installed successfully,
     * providing access to the [WebExtension] object for bi-directional messaging.
     * @param onError (optional) callback invoked if there was an error installing the extension.
     * This callback is invoked with an [UnsupportedOperationException] in case the engine doesn't
     * have web extension support.
     */
    fun installWebExtension(
        id: String,
        url: String,
        allowContentMessaging: Boolean = true,
        onSuccess: ((WebExtension) -> Unit) = { },
        onError: ((String, Throwable) -> Unit) = { _, _ -> }
    ): Unit = onError(id, UnsupportedOperationException("Web extension support is not available in this engine"))

    /**
     * Clears browsing data stored by the engine.
     *
     * @param data the type of data that should be cleared, defaults to all.
     * @param host (optional) name of the host for which data should be cleared. If
     * omitted data will be cleared for all hosts.
     * @param onSuccess (optional) callback invoked if the data was cleared successfully.
     * @param onError (optional) callback invoked if clearing the data caused an exception.
     */
    fun clearData(
        data: BrowsingData = BrowsingData.all(),
        host: String? = null,
        onSuccess: (() -> Unit) = { },
        onError: ((Throwable) -> Unit) = { }
    ): Unit = onError(UnsupportedOperationException("Clearing browsing data is not supported by this engine. " +
            "Check both the engine and engine session implementation."))

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
        onError: (Throwable) -> Unit = { }
    ): Unit = onError(
        UnsupportedOperationException(
            "getTrackersLog is not supported by this engine."
        )
    )

    /**
     * Provides access to the tracking protection exception list for this engine.
     */
    val trackingProtectionExceptionStore: TrackingProtectionExceptionStorage
        get() = throw UnsupportedOperationException("TrackingProtectionExceptionStorage not supported by this engine.")

    /**
     * Provides access to the settings of this engine.
     */
    val settings: Settings

    /**
     * Returns the version of the engine as [EngineVersion] object.
     */
    val version: EngineVersion
}
