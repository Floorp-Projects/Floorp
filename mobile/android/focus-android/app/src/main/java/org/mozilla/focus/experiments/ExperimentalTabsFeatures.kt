/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.experiments

import org.mozilla.experiments.nimbus.Variables

private const val VARIABLE_IS_MULTI_TAB = "is_multi_tab"

/**
 * Experimental features regarding the usage of "browser tabs".
 */
class ExperimentalTabsFeatures(
    variables: Variables
) {
    /**
     * Do we support multiple tabs or is this a single tab browser?
     */
    val isMultiTab: Boolean by lazy {
        variables.getBool(VARIABLE_IS_MULTI_TAB) ?: true
    }
}
