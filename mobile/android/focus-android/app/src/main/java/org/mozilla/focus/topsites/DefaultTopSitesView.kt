/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.view.TopSitesView
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore

/**
 * The default implementation of [TopSitesView] for displaying the top site UI.
 *
 * @property store [AppStore] instance for dispatching the top sites changes.
 */
class DefaultTopSitesView(private val store: AppStore) : TopSitesView {

    override fun displayTopSites(topSites: List<TopSite>) {
        store.dispatch(AppAction.TopSitesChange(topSites))
    }
}
