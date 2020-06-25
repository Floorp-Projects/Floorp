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
     * Allows edit of saved logins.
     */
    val loginsEdit = Config.channel.isNightlyOrDebug

    /**
     * Enable tab sync feature
     */
    const val syncedTabs = true

    /**
     * Enables new tab tray pref
     */
    val tabTray = Config.channel.isNightlyOrDebug

    /**
     * Enables swipe on toolbar to switch tabs
     */
    val swipeToSwitchTabs = Config.channel.isNightlyOrDebug
}
