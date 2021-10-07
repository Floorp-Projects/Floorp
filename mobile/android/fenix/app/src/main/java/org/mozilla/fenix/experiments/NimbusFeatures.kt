/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import android.content.Context
import org.mozilla.experiments.nimbus.mapKeysAsEnums
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getVariables

/**
 * Component for exposing nimbus Feature Variables.
 * For more information see https://experimenter.info/feature-variables-and-me
 *
 * @param context - A [Context] for accessing the feature variables from nimbus.
 */
class NimbusFeatures(private val context: Context) {

    val homeScreen: HomeScreenFeatures by lazy {
        HomeScreenFeatures(context)
    }

    /**
     * Component that indicates which features should be active on the home screen.
     */
    class HomeScreenFeatures(private val context: Context) {
        /**
         * `FeatureId.HOME_PAGE` feature; the complete JSON, is shown here:
         *
         * ```json
         * {
         *     "sections-enabled": {
         *         "topSites": true,
         *         "recentlySaved": false,
         *         "jumpBackIn": false,
         *         "pocket": false,
         *         "recentExplorations": false
         *     }
         * }
         * ```
         */

        /**
         * This enum accompanies the `FeatureId.HOME_PAGE` feature.
         *
         * These names here should match the names of entries in the JSON.
         */
        @Suppress("EnumNaming")
        private enum class HomeScreenSection(val default: Boolean) {
            topSites(true),
            recentlySaved(true),
            jumpBackIn(true),
            pocket(true),
            recentExplorations(true);

            companion object {
                /**
                 * CreateS a map with the corresponding default values for each sections.
                 */
                fun toMap(context: Context): Map<HomeScreenSection, Boolean> {
                    return values().associate { section ->
                        val value = if (section == pocket) {
                            FeatureFlags.isPocketRecommendationsFeatureEnabled(context)
                        } else {
                            section.default
                        }
                        section to value
                    }
                }
            }
        }

        private val homeScreenFeatures: Map<HomeScreenSection, Boolean> by lazy {
            val experiments = context.components.analytics.experiments
            val variables = experiments.getVariables(FeatureId.HOME_PAGE, false)
            val sections: Map<HomeScreenSection, Boolean> =
                variables.getBoolMap("sections-enabled")?.mapKeysAsEnums()
                    ?: HomeScreenSection.toMap(context)
            sections
        }

        /**
         * Indicates if the recently tabs feature is active.
         */
        fun isRecentlyTabsActive(): Boolean {
            return homeScreenFeatures[HomeScreenSection.jumpBackIn] == true
        }

        /**
         * Indicates if the recently saved feature is active.
         */
        fun isRecentlySavedActive(): Boolean {
            return homeScreenFeatures[HomeScreenSection.recentlySaved] == true
        }

        /**
         * Indicates if the recently exploration feature is active.
         */
        fun isRecentExplorationsActive(): Boolean {
            return homeScreenFeatures[HomeScreenSection.recentExplorations] == true
        }

        /**
         * Indicates if the pocket recommendations feature is active.
         */
        fun isPocketRecommendationsActive(): Boolean {
            return homeScreenFeatures[HomeScreenSection.pocket] == true
        }

        /**
         * Indicates if the top sites feature is active.
         */
        fun isTopSitesActive(): Boolean {
            return homeScreenFeatures[HomeScreenSection.topSites] == true
        }
    }
}
