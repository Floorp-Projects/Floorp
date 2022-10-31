/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.search

/**
 * Data class keeping track of the region of the user.
 *
 * @param home The "home" region of the user, which will change only if the user stays in the same
 * region for an extended time.
 * @param current The "current" region of the user. May change more frequently and may eventually
 * become the new "home" region after some time.
 */
data class RegionState(
    val home: String,
    val current: String,
) {
    companion object {
        /**
         * The default region when the region of the user could not be detected.
         */
        val Default = RegionState("XX", "XX")
    }
}
