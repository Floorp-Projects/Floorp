/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import mozilla.components.feature.top.sites.presenter.TopSitesPresenter
import mozilla.components.feature.top.sites.view.TopSitesView
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class TopSitesFeatureTest {

    private val view: TopSitesView = mock()
    private val storage: TopSitesStorage = mock()
    private val presenter: TopSitesPresenter = mock()
    private val config: () -> TopSitesConfig = mock()
    private val feature: TopSitesFeature = TopSitesFeature(view, storage, config, presenter)

    @Test
    fun start() {
        feature.start()

        verify(presenter).start()
    }

    @Test
    fun stop() {
        feature.stop()

        verify(presenter).stop()
    }
}
