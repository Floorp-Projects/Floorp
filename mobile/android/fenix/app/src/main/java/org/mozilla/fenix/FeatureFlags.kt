package org.mozilla.fenix

/**
 * A single source for setting feature flags that are mostly based on build type.
 */
object FeatureFlags {
    // lazy is used to suppress "Condition is always 'true'" warnings when using the flags.
    // https://github.com/mozilla-mobile/fenix/pull/4077#issuecomment-511964072

    // A convenience flag for production builds.
    private val production by lazy { BuildConfig.BUILD_TYPE == "fenixProduction" }
    // A convenience flag for beta builds.
    private val beta by lazy { BuildConfig.BUILD_TYPE == "fenixBeta" }
    // A convenience flag for the nightly build in Google Play.
    private val nightly by lazy {
        BuildConfig.BUILD_TYPE == "fenixNightly"
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
     * Gives option in Settings to disable auto play media
     */
    val autoPlayMedia = nightly or debug

    /**
     * Allows Progressive Web Apps to be installed to the device home screen.
     */
    val progressiveWebApps = nightly or debug
}
