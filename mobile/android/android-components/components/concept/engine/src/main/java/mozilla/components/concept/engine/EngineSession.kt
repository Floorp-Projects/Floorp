/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.content.Intent
import android.graphics.Bitmap
import androidx.annotation.CallSuper
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.Companion.RECOMMENDED
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ALL
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryItem
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
) : Observable<EngineSession.Observer> by delegate, DataCleanable {
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
        fun onNavigateBack() = Unit

        /**
         * Event to indicate whether or not this [EngineSession] should be [excluded] from tracking protection.
         */
        fun onExcludedOnTrackingProtectionChange(excluded: Boolean) = Unit

        /**
         * Event to indicate that this session has had it's first engine contentful paint of page content.
         */
        fun onFirstContentfulPaint() = Unit
        fun onLongPress(hitResult: HitResult) = Unit
        fun onDesktopModeChange(enabled: Boolean) = Unit
        fun onFind(text: String) = Unit
        fun onFindResult(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) = Unit
        fun onFullScreenChange(enabled: Boolean) = Unit

        /**
         * @param layoutInDisplayCutoutMode value of defined in https://developer.android.com/reference/android/view/WindowManager.LayoutParams#layoutInDisplayCutoutMode
         */
        fun onMetaViewportFitChanged(layoutInDisplayCutoutMode: Int) = Unit
        fun onThumbnailChange(bitmap: Bitmap?) = Unit
        fun onAppPermissionRequest(permissionRequest: PermissionRequest) = permissionRequest.reject()
        fun onContentPermissionRequest(permissionRequest: PermissionRequest) = permissionRequest.reject()
        fun onCancelContentPermissionRequest(permissionRequest: PermissionRequest) = Unit
        fun onPromptRequest(promptRequest: PromptRequest) = Unit

        /**
         * The engine received a request to open or close a window.
         *
         * @param windowRequest the request to describing the required window action.
         */
        fun onWindowRequest(windowRequest: WindowRequest) = Unit

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
         *
         * Unlike the name LoadRequest.isRedirect may imply this flag is not about http redirects.
         * The flag is "True if and only if the request was triggered by an HTTP redirect."
         * See: https://bugzilla.mozilla.org/show_bug.cgi?id=1545170
         */
        fun onLoadRequest(
            url: String,
            triggeredByRedirect: Boolean,
            triggeredByWebContent: Boolean
        ) = Unit

        /**
         * The engine received a request to launch a app intent.
         *
         * @param url The string url that was requested.
         * @param appIntent The Android Intent that was requested.
         * web content (as opposed to via the browser chrome).
         */
        fun onLaunchIntentRequest(
            url: String,
            appIntent: Intent?
        ) = Unit

        @Suppress("LongParameterList")
        fun onExternalResource(
            url: String,
            fileName: String? = null,
            contentLength: Long? = null,
            contentType: String? = null,
            cookie: String? = null,
            userAgent: String? = null
        ) = Unit

        /**
         * Event to indicate that this session has changed its history state.
         *
         * @param historyList The list of items in the session history.
         * @param currentIndex Index of the current page in the history list.
         */
        fun onHistoryStateChanged(historyList: List<HistoryItem>, currentIndex: Int) = Unit
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
        val cookiePolicy: CookiePolicy = ACCEPT_NON_TRACKERS,
        val strictSocialTrackingProtection: Boolean? = null
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
             * Blocks advertisement trackers from the ads-track-digest256 list.
             */
            AD(1 shl 1),

            /**
             * Blocks analytics trackers from the analytics-track-digest256 list.
             */
            ANALYTICS(1 shl 2),

            /**
             * Blocks social trackers from the social-track-digest256 list.
             */
            SOCIAL(1 shl 3),

            /**
             * Blocks content trackers from the content-track-digest256 list.
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

            /**
             * Blocks social trackers from the social-tracking-protection-digest256 list.
             */
            MOZILLA_SOCIAL(1 shl 8),

            /**
             * Blocks content like scripts and sub-resources.
             */
            SCRIPTS_AND_SUB_RESOURCES(1 shl 31),

            RECOMMENDED(AD.id + ANALYTICS.id + SOCIAL.id + TEST.id + MOZILLA_SOCIAL.id +
                CRYPTOMINING.id + FINGERPRINTING.id),

            /**
             * Combining the [RECOMMENDED] categories plus [SCRIPTS_AND_SUB_RESOURCES].
             */
            STRICT(RECOMMENDED.id + SCRIPTS_AND_SUB_RESOURCES.id)
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
                cookiePolicy = ACCEPT_NON_TRACKERS,
                strictSocialTrackingProtection = true
            )

            /**
             * Recommended policy.
             * Combining the [TrackingCategory.RECOMMENDED] plus a [CookiePolicy] of [ACCEPT_NON_TRACKERS].
             * This is the recommended setting.
             */
            fun recommended() = TrackingProtectionPolicyForSessionTypes(
                trackingCategory = arrayOf(TrackingCategory.RECOMMENDED),
                cookiePolicy = ACCEPT_NON_TRACKERS,
                strictSocialTrackingProtection = false
            )

            fun select(
                trackingCategories: Array<TrackingCategory> = arrayOf(TrackingCategory.RECOMMENDED),
                cookiePolicy: CookiePolicy = ACCEPT_NON_TRACKERS,
                strictSocialTrackingProtection: Boolean? = null
            ) = TrackingProtectionPolicyForSessionTypes(
                trackingCategories,
                cookiePolicy,
                strictSocialTrackingProtection
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
     *  @param trackingCategory a list of tracking categories to apply.
     *  @param cookiePolicy indicate how cookies should behave for this policy.
     *  @param strictSocialTrackingProtection indicate  if content should be blocked from the
     *  social-tracking-protection-digest256 list, when given a null value,
     *  it is only applied when the [EngineSession.TrackingProtectionPolicy.TrackingCategory.STRICT]
     *  is set.
     */
    class TrackingProtectionPolicyForSessionTypes internal constructor(
        trackingCategory: Array<TrackingCategory> = arrayOf(TrackingCategory.RECOMMENDED),
        cookiePolicy: CookiePolicy = ACCEPT_NON_TRACKERS,
        strictSocialTrackingProtection: Boolean? = null
    ) : TrackingProtectionPolicy(
        trackingCategories = trackingCategory,
        cookiePolicy = cookiePolicy,
        strictSocialTrackingProtection = strictSocialTrackingProtection
    ) {
        /**
         * Marks this policy to be used for private sessions only.
         */
        fun forPrivateSessionsOnly() = TrackingProtectionPolicy(
            trackingCategories = trackingCategories,
            useForPrivateSessions = true,
            useForRegularSessions = false,
            cookiePolicy = cookiePolicy,
            strictSocialTrackingProtection = strictSocialTrackingProtection
        )

        /**
         * Marks this policy to be used for regular (non-private) sessions only.
         */
        fun forRegularSessionsOnly() = TrackingProtectionPolicy(
            trackingCategories = trackingCategories,
            useForPrivateSessions = false,
            useForRegularSessions = true,
            cookiePolicy = cookiePolicy,
            strictSocialTrackingProtection = strictSocialTrackingProtection
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
            const val LOAD_FLAGS_FORCE_ALLOW_DATA_URI: Int = 1 shl 5
            const val LOAD_FLAGS_REPLACE_HISTORY: Int = 1 shl 6
            internal const val ALL = BYPASS_CACHE + BYPASS_PROXY + EXTERNAL + ALLOW_POPUPS +
                BYPASS_CLASSIFIER + LOAD_FLAGS_FORCE_ALLOW_DATA_URI + LOAD_FLAGS_REPLACE_HISTORY

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
     * @param parent the parent (referring) [EngineSession] i.e. the session that
     * triggered creating this one.
     * @param flags the [LoadUrlFlags] to use when loading the provided url.
     * @param additionalHeaders the extra headers to use when loading the provided url.
     */
    abstract fun loadUrl(
        url: String,
        parent: EngineSession? = null,
        flags: LoadUrlFlags = LoadUrlFlags.none(),
        additionalHeaders: Map<String, String>? = null
    )

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
     *
     * @param flags the [LoadUrlFlags] to use when reloading the current url.
     */
    abstract fun reload(flags: LoadUrlFlags = LoadUrlFlags.none())

    /**
     * Navigates back in the history of this session.
     */
    abstract fun goBack()

    /**
     * Navigates forward in the history of this session.
     */
    abstract fun goForward()

    /**
     * Navigates to the specified index in the [HistoryState] of this session. The current index of
     * this session's [HistoryState] will be updated but the items within it will be unchanged.
     * Invalid index values are ignored.
     *
     * @param index the index of the session's [HistoryState] to navigate to
     */
    abstract fun goToHistoryIndex(index: Int)

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
     * @return true if the engine session has successfully been restored with the provided state,
     * false otherwise.
     */
    abstract fun restoreState(state: EngineSessionState): Boolean

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
     * Marks this session active/inactive for web extensions to support
     * tabs.query({active: true}).
     *
     * @param active whether this session should be marked as active or inactive.
     */
    open fun markActiveForWebExtensions(active: Boolean) = Unit

    /**
     * Close the session. This may free underlying objects. Call this when you are finished using
     * this session.
     */
    @CallSuper
    open fun close() = delegate.unregisterObservers()
}
