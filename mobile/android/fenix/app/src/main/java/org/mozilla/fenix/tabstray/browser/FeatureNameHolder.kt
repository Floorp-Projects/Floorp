/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

/**
 * Contains the identifying name of the feature.
 *
 * This is commonly used for telemetry.
 */
interface FeatureNameHolder {
    val featureName: String
}
