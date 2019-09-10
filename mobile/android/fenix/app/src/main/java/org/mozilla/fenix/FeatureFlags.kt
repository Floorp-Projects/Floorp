package org.mozilla.fenix

/**
 * A single source for setting feature flags that are mostly based on build type.
 */
object FeatureFlags {
    // A convenience flag for production builds.
    private val production by lazy { BuildConfig.BUILD_TYPE == "fenixProduction" }
    // A convenience flag for beta builds.
    private val beta by lazy { BuildConfig.BUILD_TYPE == "fenixBeta" }
    // A convenience flag for the nightly build and (legacy) nightly channel in Google Play.
    private val nightly by lazy {
        BuildConfig.BUILD_TYPE == "fenixNightly" || BuildConfig.BUILD_TYPE == "fenixNightlyLegacy"
    }
    // A convenience flag for debug builds.
    private val debug by lazy { BuildConfig.BUILD_TYPE == "debug" }
    // A convenience flag for enabling in all builds (a feature that can still be toggled off).
    private val all = production or beta or nightly or debug

    /**
     * Pull-to-refresh allows you to pull the web content down far enough to have the page to
     * reload.
     */
    const val pullToRefreshEnabled = false

    /**
     * Integration of media features provided by `feature-media` component:
     * - Background playback without the app getting killed
     * - Media notification with play/pause controls
     * - Audio Focus handling (pausing/resuming in agreement with other media apps)
     * - Support for hardware controls to toggle play/pause (e.g. buttons on a headset)
     *
     * Behind nightly flag until all related Android Components issues are fixed and QA has signed
     * off.
     *
     * https://github.com/mozilla-mobile/fenix/issues/4431
     */
    const val mediaIntegration = true

    /**
     * Displays the categories blocked by ETP in a panel in the toolbar
     */
    val etpCategories = nightly or debug

    /**
     * Granular data deletion provides additional choices on the Delete Browsing Data
     * setting screen for cookies, cached images and files, and site permissions.
     */
    val granularDataDeletion = nightly or debug
}
