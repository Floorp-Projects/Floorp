/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import mozilla.components.feature.syncedtabs.interactor.SyncedTabsInteractor
import mozilla.components.feature.syncedtabs.presenter.SyncedTabsPresenter
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class SyncedTabsFeatureTest {

    private val context: Context = mock()
    private val storage: SyncedTabsStorage = mock()
    private val accountManager: FxaAccountManager = mock()
    private val view: SyncedTabsView = mock()
    private val lifecycleOwner: LifecycleOwner = mock()

    private val presenter: SyncedTabsPresenter = mock()
    private val interactor: SyncedTabsInteractor = mock()
    private val feature: SyncedTabsFeature =
        SyncedTabsFeature(
            context,
            storage,
            accountManager,
            view,
            lifecycleOwner,
            onTabClicked = {},
            presenter = presenter,
            interactor = interactor,
        )

    @Test
    fun start() {
        feature.start()

        verify(presenter).start()
        verify(interactor).start()
    }

    @Test
    fun stop() {
        feature.stop()

        verify(presenter).stop()
        verify(interactor).stop()
    }
}
