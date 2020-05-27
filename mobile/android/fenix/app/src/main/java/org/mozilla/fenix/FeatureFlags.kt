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
     * Disables FxA Application Services Web Channels feature
     */
    const val asFeatureWebChannelsDisabled = false

    /**
     * Disables FxA Application Services Sync feature
     */
    const val asFeatureSyncDisabled = false

    /**
     * Enables dynamic bottom toolbar
     */
    val dynamicBottomToolbar = Config.channel.isNightlyOrDebug

    /**
     * Integration of push support provided by `feature-push` component into the Gecko engine.
     *
     * Behind nightly flag until all fatal bugs are resolved.
     *
     * https://github.com/mozilla-mobile/fenix/issues/9059
     */
    const val webPushIntegration = true

    /**
     * Enables tip feature
     */
    val tips = Config.channel.isDebug

    /**
     * Allows edit of saved logins.
     */
    val loginsEdit = Config.channel.isNightlyOrDebug

    /**
     * Enables new tab tray pref
     */
    val tabTray = Config.channel.isNightlyOrDebug

    /**
     * Enables new voice search feature
     */
    val voiceSearch = Config.channel.isNightlyOrDebug

    /**
     * Allows search widget CFR to be displayed.
     * This is a placeholder for the experimentation framework determining cohorts.
     */
    val searchWidgetCFR = Config.channel.isDebug
}
