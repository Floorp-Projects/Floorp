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
     * Send Tab is a feature to lets you send a url/tab from a desktop to device and vice versa.
     *
     * NB: flipping this flag back and worth is currently not well supported and may need
     * hand-holding. Consult with the android-components peers before changing.
     *
     * This flag is temporarily also used for the push service that is requires it to.
     * See: https://github.com/mozilla-mobile/fenix/issues/4063
     */
    val sendTabEnabled = all

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
    val mediaIntegration = nightly or debug
}
