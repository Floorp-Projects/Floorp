/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

/**
 * Enums to identify features in the app. These will likely grow and shrink depending
 * on the experiments we want to perform.
 *
 * @property jsonName the kebab-case version of the feature id as represented in the Nimbus
 * experiment JSON.
 */
enum class FeatureId(val jsonName: String) {
    NIMBUS_VALIDATION("nimbus-validation"),
    ANDROID_KEYSTORE("fenix-android-keystore"),
    DEFAULT_BROWSER("fenix-default-browser")
}

/**
 * Experiment branches are becoming less interesting, though we collect some well
 * defined ones here.
 */
class ExperimentBranch {
    companion object {
        const val TREATMENT = "treatment"
        const val CONTROL = "control"
        const val A1 = "a1"
        const val A2 = "a2"
        const val DEFAULT_BROWSER_TOOLBAR_MENU = "default_browser_toolbar_menu"
        const val DEFAULT_BROWSER_NEW_TAB_BANNER = "default_browser_newtab_banner"
        const val DEFAULT_BROWSER_SETTINGS_MENU = "default_browser_settings_menu"
    }
}
