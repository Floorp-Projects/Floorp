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
     * Enables tip feature
     */
    val tips = Config.channel.isDebug

    /**
     * Allows edit of saved logins.
     */
    val loginsEdit = Config.channel.isNightlyOrDebug

    /**
     * Enable tab sync feature
     */
    val syncedTabs = Config.channel.isNightlyOrDebug

    /**
     * Enables new tab tray pref
     */
    val tabTray = Config.channel.isNightlyOrDebug

    /**
     * Allows search widget CFR to be displayed.
     * This is a placeholder for the experimentation framework determining cohorts.
     */
    val searchWidgetCFR = Config.channel.isDebug
}
