/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.view

import mozilla.components.feature.top.sites.TopSite

/**
 * Implemented by the application for displaying onto the UI.
 */
interface TopSitesView {
    /**
     * Updates the UI with new list of top sites.
     */
    fun displayTopSites(topSites: List<TopSite>)
}
