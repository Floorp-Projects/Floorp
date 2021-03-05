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
     * Enables the Nimbus experiments library.
     */
    val nimbusExperiments = Config.channel.isNightlyOrDebug

    /**
     * Enables the Addresses autofill feature.
     */
    val addressesFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables the Credit Cards autofill feature.
     */
    val creditCardsFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables WebAuthn support.
     */
    val webAuthFeature = Config.channel.isNightlyOrDebug

    /**
     * Shows new three-dot toolbar menu design.
     */
    val toolbarMenuFeature = Config.channel.isDebug

    /**
     * Enables the tabs tray re-write with Synced Tabs.
     */
    val tabsTrayRewrite = Config.channel.isNightlyOrDebug

    /**
     * Enables the updated icon set look and feel.
     */
    val newIconSet = Config.channel.isNightlyOrDebug
}
