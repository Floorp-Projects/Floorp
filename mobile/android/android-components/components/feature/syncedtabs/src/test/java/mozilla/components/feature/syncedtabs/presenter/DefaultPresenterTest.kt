/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.presenter

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.feature.syncedtabs.controller.SyncedTabsController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SyncEnginesStorage.Companion.SYNC_ENGINES_KEY
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions

@RunWith(AndroidJUnit4::class)
class DefaultPresenterTest {

    private val context: Context = testContext
    private val controller: SyncedTabsController = mock()
    private val accountManager: FxaAccountManager = mock()
    private val view: SyncedTabsView = mock()
    private val lifecycleOwner: LifecycleOwner = mock()

    private val prefs = testContext.getSharedPreferences(SYNC_ENGINES_KEY, Context.MODE_PRIVATE)

    @Test
    fun `start returns when there is no profile`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        presenter.start()

        verify(view).onError(ErrorType.SYNC_UNAVAILABLE)
    }

    @Test
    fun `start returns if sync engine is not enabled`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        // disable sync storage
        prefs.edit().putBoolean("tabs", false).apply()
        `when`(accountManager.authenticatedAccount()).thenReturn(mock())

        presenter.start()

        verify(view).onError(ErrorType.SYNC_ENGINE_UNAVAILABLE)
    }

    @Test
    fun `start returns if sync needs reauthentication`() {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        `when`(accountManager.authenticatedAccount()).thenReturn(mock())
        `when`(accountManager.accountNeedsReauth()).thenReturn(true)

        presenter.start()

        verify(view).onError(ErrorType.SYNC_NEEDS_REAUTHENTICATION)
    }

    @Test
    fun `start invokes syncTabs - account profile is absent`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        prefs.edit().putBoolean("tabs", true).apply()
        `when`(accountManager.authenticatedAccount()).thenReturn(mock())
        `when`(accountManager.accountProfile()).thenReturn(null)
        `when`(accountManager.accountNeedsReauth()).thenReturn(false)

        presenter.start()

        verify(controller).syncAccount()
    }

    @Test
    fun `start invokes syncTabs - account profile is present`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        prefs.edit().putBoolean("tabs", true).apply()
        `when`(accountManager.authenticatedAccount()).thenReturn(mock())
        `when`(accountManager.accountProfile()).thenReturn(mock())
        `when`(accountManager.accountNeedsReauth()).thenReturn(false)

        presenter.start()

        verify(controller).syncAccount()
    }

    @Test
    fun `notify on logout`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        presenter.accountObserver.onLoggedOut()

        verify(view).onError(ErrorType.SYNC_UNAVAILABLE)
    }

    @Test
    fun `notify on authenticated`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        presenter.accountObserver.onAuthenticated(mock(), mock())

        verify(controller).refreshSyncedTabs()
    }

    @Test
    fun `notify on authentication problems`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        presenter.accountObserver.onAuthenticationProblems()

        verify(view).onError(ErrorType.SYNC_NEEDS_REAUTHENTICATION)
    }

    @Test
    fun `sync tabs on idle status - tabs sync enabled`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        prefs.edit().putBoolean("tabs", true).apply()
        presenter.eventObserver.onIdle()

        verify(controller).refreshSyncedTabs()
    }

    @Test
    fun `sync tabs on idle status - tabs sync disabled`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        prefs.edit().putBoolean("tabs", false).apply()
        presenter.eventObserver.onIdle()

        verifyNoInteractions(controller)
        verify(view).onError(ErrorType.SYNC_ENGINE_UNAVAILABLE)
    }

    @Test
    fun `show loading state on started status`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        presenter.eventObserver.onStarted()

        verify(view).startLoading()
    }

    @Test
    fun `notify on error`() = runBlockingTest {
        val presenter = DefaultPresenter(
            context,
            controller,
            accountManager,
            view,
            lifecycleOwner
        )

        presenter.eventObserver.onError(mock())

        verify(view).onError(ErrorType.SYNC_ENGINE_UNAVAILABLE)
    }
}
