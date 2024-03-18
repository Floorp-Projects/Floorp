/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.presenter

import mozilla.components.feature.top.sites.TopSitesStorage
import mozilla.components.feature.top.sites.view.TopSitesView
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * A presenter that connects the [TopSitesView] with the [TopSitesStorage].
 */
interface TopSitesPresenter : LifecycleAwareFeature {
    val view: TopSitesView
    val storage: TopSitesStorage
}
