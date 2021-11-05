/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.experiments

import mozilla.components.service.nimbus.NimbusApi

private const val FEATURE_TABS = "tabs"

/**
 * Experimental features driven by Nimbus experiments.
 */
class ExperimentalFeatures(nimbus: NimbusApi) {
    val tabs: ExperimentalTabsFeatures by lazy {
        ExperimentalTabsFeatures(
            nimbus.getVariables(FEATURE_TABS)
        )
    }
}
