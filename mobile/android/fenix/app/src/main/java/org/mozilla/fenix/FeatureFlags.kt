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
    const val pullToRefreshEnabled = false

    /**
     * Allows edit of saved logins.
     */
    const val loginsEdit = true

    /**
     * Shows Synced Tabs in the tabs tray.
     *
     * Tracking issue: https://github.com/mozilla-mobile/fenix/issues/13892
     */
    val syncedTabsInTabsTray = Config.channel.isNightlyOrDebug

    /**
     * Enables viewing tab history
     */
    val tabHistory = Config.channel.isNightlyOrDebug

    /**
     * Enables the new search experience
     */
    val newSearchExperience = Config.channel.isNightlyOrDebug

    /**
     * Enables wait til first contentful paint
     */
    val waitUntilPaintToDraw = Config.channel.isNightlyOrDebug

    /**
     * Enables downloads with external download managers.
     */
    val externalDownloadManager = Config.channel.isNightlyOrDebug

    /**
     * Enables viewing downloads in browser.
     */
    val viewDownloads = Config.channel.isNightlyOrDebug
}
