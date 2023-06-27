/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.nimbus.FxNimbus

/**
 * Feature implementation that provides review quality check information for supported product
 * pages.
 */
class ReviewQualityCheckFeature(
    private val onAvailabilityChange: (isAvailable: Boolean) -> Unit,
) : LifecycleAwareFeature {

    override fun start() {
        val isFeatureEnabled = FxNimbus.features.shoppingExperience.value().enabled
        // Update to use product page detector api in Bug 1840580
        val isSupportedProductPage = false
        onAvailabilityChange(isFeatureEnabled && isSupportedProductPage)
    }

    override fun stop() {
        // no-op
    }
}
