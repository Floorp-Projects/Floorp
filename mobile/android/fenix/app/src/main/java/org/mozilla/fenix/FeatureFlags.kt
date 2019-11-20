package org.mozilla.fenix

/**
 * A single source for setting feature flags that are mostly based on build type.
 */
object FeatureFlags {
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
     * Gives option in Settings to disable auto play media
     */
    val autoPlayMedia = Config.channel.isNightlyOrDebug

    /**
     * Allows Progressive Web Apps to be installed to the device home screen.
     */
    val progressiveWebApps = Config.channel.isNightlyOrDebug

    /**
     * Gives option in Settings to see logins and sync logins
     */
    val logins = Config.channel.isNightlyOrDebug
}
