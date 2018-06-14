/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Interface used to provide
 * the user's region for evaluating
 * experiments
 */
interface RegionProvider {
    /**
     * Provides the user's region
     *
     * @return user's region
     */
    fun getRegion(): String
}
