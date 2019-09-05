/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.graphics.Bitmap
import androidx.annotation.CallSuper
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.Companion.RECOMMENDED
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ALL
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * Class representing a single engine session.
 *
 * In browsers usually a session corresponds to a tab.
 */
@Suppress("TooManyFunctions")
abstract class EngineSession(
    private val delegate: Observable<EngineSession.Observer> = ObserverRegistry()
) : Observable<EngineSession.Observer> by delegate {
    /**
     * Interface to be implemented by classes that want to observe this engine session.
     */
    interface Observer {
        fun onLocationChange(url: String) = Unit
        fun onTitleChange(title: String) = Unit
        fun onProgress(progress: Int) = Unit
        fun onLoadingStateChange(loading: Boolean) = Unit
        fun onNavigationStateChange(canGoBack: Boolean? = null, canGoForward: Boolean? = null) = Unit
        fun onSecurityChange(secure: Boolean, host: String? = null, issuer: String? = null) = Unit
        fun onTrackerBlockingEnabledChange(enabled: Boolean) = Unit
        fun onTrackerBlocked(tracker: Tracker) = Unit
        fun onTrackerLoaded(tracker: Tracker) = Unit
        fun onLongPress(hitResult: HitResult) = Unit
        fun onDesktopModeChange(enabled: Boolean) = Unit
        fun onFind(text: String) = Unit
        fun onFindResult(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) = Unit
        fun onFullScreenChange(enabled: Boolean) = Unit
        fun onThumbnailChange(bitmap: Bitmap?) = Unit
        fun onAppPermissionRequest(permissionRequest: PermissionRequest) = permissionRequest.reject()
        fun onContentPermissionRequest(permissionRequest: PermissionRequest) = permissionRequest.reject()
        fun onCancelContentPermissionRequest(permissionRequest: PermissionRequest) = Unit
        fun onPromptRequest(promptRequest: PromptRequest) = Unit
        fun onOpenWindowRequest(windowRequest: WindowRequest) = Unit
        fun onCloseWindowRequest(windowRequest: WindowRequest) = Unit
        fun onMediaAdded(media: Media) = Unit
        fun onMediaRemoved(media: Media) = Unit
        fun onWebAppManifestLoaded(manifest: WebAppManifest) = Unit
        fun onCrash() = Unit
        fun onProcessKilled() = Unit
        fun onRecordingStateChanged(devices: List<RecordingDevice>) = Unit

        /**
         * The engine received a request to load a request.
         *
         * @param url The string url that was requested.
         * @param triggeredByRedirect True if and only if the request was triggered by an HTTP redirect.
         * @param triggeredByWebContent True if and only if the request was triggered from within
         * web content (as opposed to via the browser chrome).
         */
        fun onLoadRequest(url: String, triggeredByRedirect: Boolean, triggeredByWebContent: Boolean) = Unit

        @Suppress("LongParameterList")
        fun onExternalResource(
            url: String,
            fileName: String? = null,
            contentLength: Long? = null,
            contentType: String? = null,
            cookie: String? = null,
            userAgent: String? = null
        ) = Unit
    }

    /**
     * Provides access to the settings of this engine session.
     */
    abstract val settings: Settings

    /**
     * Represents a safe browsing policy, which is indicates with type of site should be alerted
     * to user as possible harmful.
     */
    @Suppress("MagicNumber")
    enum class SafeBrowsingPolicy(val id: Int) {
        NONE(0),

        /**
         * Blocks malware sites.
         */
        MALWARE(1 shl 10),

        /**
         * Blocks unwanted sites.
         */
        UNWANTED(1 shl 11),

        /**
         * Blocks harmful sites.
         */
        HARMFUL(1 shl 12),

        /**
         * Blocks phishing sites.
         */
        PHISHING(1 shl 13),

        /**
         * Blocks all unsafe sites.
         */
        RECOMMENDED(MALWARE.id + UNWANTED.id + HARMFUL.id + PHISHING.id)
    }

    /**
     * Represents a tracking protection policy, which is a combination of
     * tracker categories that should be blocked. Unless otherwise specified,
     * a [TrackingProtectionPolicy] is applicable to all session types (see
     * [TrackingProtectionPolicyForSessionTypes]).
     */
    open class TrackingProtectionPolicy internal constructor(
        val trackingCategories: Array<TrackingCategory> = arrayOf(TrackingCategory.RECOMMENDED),
        val useForPrivateSessions: Boolean = true,
        val useForRegularSessions: Boolean = true,
        val cookiePolicy: CookiePolicy = ACCEPT_NON_TRACKERS
    ) {

        /**
         * Indicates how cookies should behave for a given [TrackingProtectionPolicy].
         * The ids of each cookiePolicy is aligned with the GeckoView @CookieBehavior constants.
         */
        @Suppress("MagicNumber")
        enum class CookiePolicy(val id: Int) {
            /**
             * Accept first-party and third-party cookies and site data.
             */
            ACCEPT_ALL(0),

            /**
             * Accept only first-party cookies and site data to block cookies which are
             * not associated with the domain of the visited site.
             */
            ACCEPT_ONLY_FIRST_PARTY(1),

            /**
             * Do not store any cookies and site data.
             */
            ACCEPT_NONE(2),

            /**
             * Accept first-party and third-party cookies and site data only from
             * sites previously visited in a first-party context.
             */
            ACCEPT_VISITED(3),

            /**
             * Accept only first-party and non-tracking third-party cookies and site data
             * to block cookies which are not associated with the domain of the visited
             * site set by known trackers.
             */
            ACCEPT_NON_TRACKERS(4)
        }

        @Suppress("MagicNumber")
        enum class TrackingCategory(val id: Int) {

            NONE(0),

            /**
             * Blocks advertisement trackers.
             */
            AD(1 shl 1),

            /**
             * Blocks analytics trackers.
             */
            ANALYTICS(1 shl 2),

            /**
             * Blocks social trackers.
             */
            SOCIAL(1 shl 3),

            /**
             * Blocks content trackers.
             * May cause issues with some web sites.
             */
            CONTENT(1 shl 4),

            // This policy is just to align categories with GeckoView
            TEST(1 shl 5),

            /**
             * Blocks cryptocurrency miners.
             */
            CRYPTOMINING(1 shl 6),

            /**
             * Blocks fingerprinting trackers.
             */
            FINGERPRINTING(1 shl 7),

            RECOMMENDED(AD.id + ANALYTICS.id + SOCIAL.id + TEST.id),

            /**
             * Combining the [RECOMMENDED] categories plus [CRYPTOMINING],
             * [FINGERPRINTING] and [CONTENT].
             */
            STRICT(RECOMMENDED.id + CRYPTOMINING.id + FINGERPRINTING.id + CONTENT.id)
        }

        companion object {

            internal val RECOMMENDED = TrackingProtectionPolicy()

            fun none() = TrackingProtectionPolicy(
                trackingCategories = arrayOf(TrackingCategory.NONE),
                cookiePolicy = ACCEPT_ALL
            )

            /**
             * Strict policy.
             * Combining the [TrackingCategory.STRICT] plus a cookiePolicy of [ACCEPT_NON_TRACKERS]
             * This is the strictest setting and may cause issues on some web sites.
             */
            fun strict() = TrackingProtectionPolicyForSessionTypes(
                trackingCategory = arrayOf(TrackingCategory.STRICT),
                cookiePolicy = ACCEPT_NON_TRACKERS
            )

            /**
             * Recommended policy.
             * Combining the [TrackingCategory.RECOMMENDED] plus a [CookiePolicy] of [ACCEPT_NON_TRACKERS].
             * This is the recommended setting.
             */
            fun recommended() = TrackingProtectionPolicyForSessionTypes(
                trackingCategory = arrayOf(TrackingCategory.RECOMMENDED),
                cookiePolicy = ACCEPT_NON_TRACKERS
            )

            fun select(
                trackingCategories: Array<TrackingCategory> = arrayOf(TrackingCategory.RECOMMENDED),
                cookiePolicy: CookiePolicy = ACCEPT_NON_TRACKERS
            ) = TrackingProtectionPolicyForSessionTypes(
                trackingCategories,
                cookiePolicy
            )
        }

        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is TrackingProtectionPolicy) return false
            if (hashCode() != other.hashCode()) return false
            if (useForPrivateSessions != other.useForPrivateSessions) return false
            if (useForRegularSessions != other.useForRegularSessions) return false
            return true
        }

        override fun hashCode() = trackingCategories.sumBy { it.id } + cookiePolicy.id

        fun contains(category: TrackingCategory) =
            (trackingCategories.sumBy { it.id } and category.id) != 0
    }

    /**
     * Subtype of [TrackingProtectionPolicy] to control the type of session this policy
     * should be applied to. By default, a policy will be applied to all sessions.
     */
    class TrackingProtectionPolicyForSessionTypes internal constructor(
        trackingCategory: Array<TrackingCategory> = arrayOf(TrackingCategory.RECOMMENDED),
        cookiePolicy: CookiePolicy = ACCEPT_NON_TRACKERS
    ) : TrackingProtectionPolicy(
        trackingCategories = trackingCategory,
        cookiePolicy = cookiePolicy
    ) {
        /**
         * Marks this policy to be used for private sessions only.
         */
        fun forPrivateSessionsOnly() = TrackingProtectionPolicy(
            trackingCategories = trackingCategories,
            useForPrivateSessions = true,
            useForRegularSessions = false,
            cookiePolicy = cookiePolicy
        )

        /**
         * Marks this policy to be used for regular (non-private) sessions only.
         */
        fun forRegularSessionsOnly() = TrackingProtectionPolicy(
            trackingCategories = trackingCategories,
            useForPrivateSessions = false,
            useForRegularSessions = true,
            cookiePolicy = cookiePolicy
        )
    }

    /**
     * Describes a combination of flags provided to the engine when loading a URL.
     */
    class LoadUrlFlags internal constructor(val value: Int) {
        companion object {
            const val NONE: Int = 0
            const val BYPASS_CACHE: Int = 1 shl 0
            const val BYPASS_PROXY: Int = 1 shl 1
            const val EXTERNAL: Int = 1 shl 2
            const val ALLOW_POPUPS: Int = 1 shl 3
            const val BYPASS_CLASSIFIER: Int = 1 shl 4
            internal const val ALL = BYPASS_CACHE + BYPASS_PROXY + EXTERNAL + ALLOW_POPUPS + BYPASS_CLASSIFIER

            fun all() = LoadUrlFlags(ALL)
            fun none() = LoadUrlFlags(NONE)
            fun external() = LoadUrlFlags(EXTERNAL)
            fun select(vararg types: Int) = LoadUrlFlags(types.sum())
        }

        fun contains(flag: Int) = (value and flag) != 0

        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is LoadUrlFlags) return false
            if (value != other.value) return false
            return true
        }

        override fun hashCode() = value
    }

    /**
     * Loads the given URL.
     *
     * @param url the url to load.
     * @param flags the [LoadUrlFlags] to use when loading the provider url.
     */
    abstract fun loadUrl(url: String, flags: LoadUrlFlags = LoadUrlFlags.none())

    /**
     * Loads the data with the given mimeType.
     * Example:
     * ```
     * engineSession.loadData("<html><body>Example HTML content here</body></html>", "text/html")
     * ```
     *
     * If the data is base64 encoded, you can override the default encoding (UTF-8) with 'base64'.
     * Example:
     * ```
     * engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
     * ```
     *
     * @param data The data that should be rendering.
     * @param mimeType the data type needed by the engine to know how to render it.
     * @param encoding specifies whether the data is base64 encoded; use 'base64' else defaults to "UTF-8".
     */
    abstract fun loadData(data: String, mimeType: String = "text/html", encoding: String = "UTF-8")

    /**
     * Stops loading the current session.
     */
    abstract fun stopLoading()

    /**
     * Reloads the current URL.
     */
    abstract fun reload()

    /**
     * Navigates back in the history of this session.
     */
    abstract fun goBack()

    /**
     * Navigates forward in the history of this session.
     */
    abstract fun goForward()

    /**
     * Saves and returns the engine state. Engine implementations are not required
     * to persist the state anywhere else than in the returned map. Engines that
     * already provide a serialized state can use a single entry in this map to
     * provide this state. The only requirement is that the same map can be used
     * to restore the original state. See [restoreState] and the specific
     * engine implementation for details.
     */
    abstract fun saveState(): EngineSessionState

    /**
     * Restores the engine state as provided by [saveState].
     *
     * @param state state retrieved from [saveState]
     */
    abstract fun restoreState(state: EngineSessionState)

    /**
     * Enables tracking protection for this engine session.
     *
     * @param policy the tracking protection policy to use, defaults to blocking all trackers.
     */
    abstract fun enableTrackingProtection(policy: TrackingProtectionPolicy = TrackingProtectionPolicy.strict())

    /**
     * Disables tracking protection for this engine session.
     */
    abstract fun disableTrackingProtection()

    /**
     * Enables/disables Desktop Mode with an optional ability to reload the session right after.
     */
    abstract fun toggleDesktopMode(enable: Boolean, reload: Boolean = false)

    /**
     * Clears browsing data stored by the engine.
     *
     * @param data the type of data that should be cleared.
     * @param host (optional) name of the host for which data should be cleared. If
     * omitted data will be cleared for all hosts.
     * @param onSuccess (optional) callback invoked if the data was cleared successfully.
     * @param onError (optional) callback invoked if clearing the data caused an exception.
     */
    open fun clearData(
        data: Engine.BrowsingData = Engine.BrowsingData.all(),
        host: String? = null,
        onSuccess: (() -> Unit) = { },
        onError: ((Throwable) -> Unit) = { }
    ): Unit = onError(UnsupportedOperationException("Clearing browsing data is not supported by this engine. " +
            "Check both the engine and engine session implementation."))

    /**
     * Finds and highlights all occurrences of the provided String and highlights them asynchronously.
     *
     * @param text the String to search for
     */
    abstract fun findAll(text: String)

    /**
     * Finds and highlights the next or previous match found by [findAll].
     *
     * @param forward true if the next match should be highlighted, false for
     * the previous match.
     */
    abstract fun findNext(forward: Boolean)

    /**
     * Clears the highlighted results of previous calls to [findAll] / [findNext].
     */
    abstract fun clearFindMatches()

    /**
     * Exits fullscreen mode if currently in it that state.
     */
    abstract fun exitFullScreenMode()

    /**
     * Tries to recover from a crash by restoring the last know state.
     *
     * Returns true if a last known state was restored, otherwise false.
     */
    abstract fun recoverFromCrash(): Boolean

    /**
     * Close the session. This may free underlying objects. Call this when you are finished using
     * this session.
     */
    @CallSuper
    open fun close() = delegate.unregisterObservers()
}
