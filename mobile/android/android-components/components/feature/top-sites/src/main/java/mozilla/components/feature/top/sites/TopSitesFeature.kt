/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import mozilla.components.feature.top.sites.presenter.DefaultTopSitesPresenter
import mozilla.components.feature.top.sites.presenter.TopSitesPresenter
import mozilla.components.feature.top.sites.view.TopSitesView
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * View-bound feature that updates the UI when the [TopSitesStorage] is updated.
 *
 * @param view An implementor of [TopSitesView] that will be notified of changes to the storage.
 * @param storage The top sites storage that stores pinned and frecent sites.
 * @param config Lambda expression that returns [TopSitesConfig] which species the number of top
 * sites to return and whether or not to include frequently visited sites.
 */
class TopSitesFeature(
    private val view: TopSitesView,
    val storage: TopSitesStorage,
    val config: () -> TopSitesConfig,
    private val presenter: TopSitesPresenter = DefaultTopSitesPresenter(
        view,
        storage,
        config,
    ),
) : LifecycleAwareFeature {

    override fun start() {
        presenter.start()
    }

    override fun stop() {
        presenter.stop()
    }
}
