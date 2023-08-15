/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import org.mozilla.fenix.utils.Settings

/**
 * An abstraction for shopping experience feature flag.
 */
interface ShoppingExperienceFeature {

    /**
     * Returns true if the shopping experience feature is enabled.
     */
    val isEnabled: Boolean
}

/**
 * The default implementation of [ShoppingExperienceFeature].
 *
 * @property settings Used to check if the feature is enabled.
 */
class DefaultShoppingExperienceFeature(
    private val settings: Settings,
) : ShoppingExperienceFeature {

    override val isEnabled
        get() = settings.enableShoppingExperience
}
