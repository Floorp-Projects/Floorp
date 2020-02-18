/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share

/**
 * Interface used for adapting recent apps database entities
 *
 * @property packageName - unique identifier of the app
 * @property score - value used for sorting in descending order the recent apps (most recent first)
 */
interface RecentApp {

    /**
     * The packageName of the recent app.
     */
    val packageName: String

    /**
     * The score of the recent app (calculated based on number of selections - decay)
     */
    val score: Double
}
