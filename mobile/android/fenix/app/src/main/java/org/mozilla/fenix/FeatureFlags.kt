/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

/**
 * A single source for setting feature flags that are mostly based on build type.
 */
object FeatureFlags {
    /**
     * Pull-to-refresh allows you to pull the web content down far enough to have the page to
     * reload.
     */
    val pullToRefreshEnabled = Config.channel.isNightlyOrDebug

    /**
     * Shows Synced Tabs in the tabs tray.
     *
     * Tracking issue: https://github.com/mozilla-mobile/fenix/issues/13892
     */
    val syncedTabsInTabsTray = Config.channel.isNightlyOrDebug

    /**
     * Enables downloads with external download managers.
     */
    const val externalDownloadManager = true

    /**
     * Enables swipe to delete in bookmarks
     */
    val bookmarkSwipeToDelete = Config.channel.isNightlyOrDebug

    /**
     * Enables ETP cookie purging
     */
    val etpCookiePurging = Config.channel.isNightlyOrDebug
}
