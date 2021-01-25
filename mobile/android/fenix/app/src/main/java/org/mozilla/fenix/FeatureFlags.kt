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
     * Enables the Nimbus experiments library, especially the settings toggle to opt-out of
     * all experiments.
     */
    // IMPORTANT: Only turn this back on once the following issues are resolved:
    // - https://github.com/mozilla-mobile/fenix/issues/17086: Calls to
    // getExperimentBranch seem to block on updateExperiments causing a
    // large performance regression loading the home screen.
    // - https://github.com/mozilla-mobile/fenix/issues/17143: Despite
    // having wrapped getExperimentBranch/withExperiments in a catch-all
    // users are still experiencing crashes.
    const val nimbusExperiments = false

    /**
     * Enables an intentional regression to validate perftest alerting. See
     * https://github.com/mozilla-mobile/fenix/issues/17447 for details. This
     * is expected to be removed within several days.
     */
    val intentionalRegressionToValidatePerfTestAlerting = Config.channel.isNightlyOrDebug

    /**
     * Enables the new MediaSession API.
     */
    @Suppress("MayBeConst")
    val newMediaSessionApi = true

    /**
     * Enables experimental WebAuthn support. This implementation should never reach release!
     */
    val webAuthFeature = Config.channel.isNightlyOrDebug
}
