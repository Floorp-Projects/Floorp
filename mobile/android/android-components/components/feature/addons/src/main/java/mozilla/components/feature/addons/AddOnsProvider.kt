/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

/**
 * A contract that indicate how an add-on provider must behave.
 */
interface AddOnsProvider {
    /**
     * Provides a list of all available add-ons.
     */
    suspend fun getAvailableAddOns(): List<AddOn>
}
