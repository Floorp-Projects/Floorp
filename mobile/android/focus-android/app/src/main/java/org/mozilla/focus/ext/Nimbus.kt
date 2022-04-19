/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import mozilla.components.service.nimbus.NimbusApi

/**
 * Nimbus feature name intended to control the multiple tabs feature in the app.
 */
const val FEATURE_TABS = "tabs"

/**
 * Nimbus variable of [FEATURE_TABS] allowing outside control of whether the multiple tabs feature
 * should be enabled or not.
 */
private const val VARIABLE_IS_MULTI_TAB = "is_multi_tab"

/**
 * Whether the multiple tabs feature support is enabled or not.
 * Defaults to `true`. May be overridden by a Nimbus experiment.
 */
val NimbusApi.isMultiTabsEnabled
    get() = getVariables(featureId = FEATURE_TABS, recordExposureEvent = false)
        .getBool(VARIABLE_IS_MULTI_TAB) ?: true
