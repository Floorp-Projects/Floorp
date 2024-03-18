/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.presenter

import mozilla.components.feature.top.sites.DefaultTopSitesStorage
import mozilla.components.feature.top.sites.TopSitesConfig
import mozilla.components.feature.top.sites.view.TopSitesView
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class DefaultTopSitesPresenterTest {

    private val view: TopSitesView = mock()
    private val storage: DefaultTopSitesStorage = mock()
    private val config: () -> TopSitesConfig = mock()
    private val presenter: DefaultTopSitesPresenter =
        spy(DefaultTopSitesPresenter(view, storage, config))

    @Test
    fun start() {
        presenter.start()

        verify(presenter).onStorageUpdated()
        verify(storage).register(presenter)
    }

    @Test
    fun stop() {
        presenter.stop()

        verify(storage).unregister(presenter)
    }
}
