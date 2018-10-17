/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

/**
 * This defines the common set of data shared across all the different
 * metric types.
 */
interface CommonMetricData {
    val applicationProperty: Boolean?
    val disabled: Boolean?
    val group: String?
    val name: String?
    val sendInPings: List<String>?
    val userProperty: Boolean?
}
