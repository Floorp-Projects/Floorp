/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.view.TopSitesView
import org.mozilla.fenix.ext.sort
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.utils.Settings

class DefaultTopSitesView(
    val store: HomeFragmentStore,
    val settings: Settings
) : TopSitesView {

    override fun displayTopSites(topSites: List<TopSite>) {
        store.dispatch(
            HomeFragmentAction.TopSitesChange(
                if (!settings.showContileFeature) {
                    topSites
                } else {
                    topSites.sort()
                }
            )
        )
    }
}
