package org.mozilla.fenix

/**
 * A single source for setting feature flags that are mostly based on build type.
 */
object FeatureFlags {
    // A convenience flag for production builds.
    private val production by lazy { BuildConfig.BUILD_TYPE == "production" }
    // A convenience flag for beta builds.
    private val beta by lazy { BuildConfig.BUILD_TYPE == "beta" }
    // A convenience flag for the nightly build and (legacy) nightly channel in Google Play.
    private val nightly by lazy { BuildConfig.BUILD_TYPE == "nightly" || BuildConfig.BUILD_TYPE == "nightlyLegacy" }
    // A convenience flag for debug builds.
    private val debug by lazy { BuildConfig.BUILD_TYPE == "debug" }

    /**
     * Send Tab is a feature to lets you send a url/tab from a desktop to device and vice versa.
     *
     * NB: flipping this flag back and worth is currently not well supported and may need
     * hand-holding. Consult with the android-components peers before changing.
     *
     * This flag is temporarily also used for the push service that is requires it to.
     * See: https://github.com/mozilla-mobile/fenix/issues/4063
     */
    val sendTabEnabled = nightly || debug

    /**
     * Pull-to-refresh allows you to pull the web content down far enough to have the page to
     * reload.
     */
    const val pullToRefreshEnabled = false
}
