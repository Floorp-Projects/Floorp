/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import androidx.lifecycle.LifecycleOwner
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.concept.sync.AuthType
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
internal class BookmarksSharedViewModelTest {

    private lateinit var viewModel: BookmarksSharedViewModel

    @Before
    fun setup() {
        viewModel = BookmarksSharedViewModel()
        viewModel.signedIn.observeForever { }
    }

    @Test
    fun `onAuthenticated and onLoggedOut modify signedIn value`() {
        viewModel.onLoggedOut()
        assertFalse(viewModel.signedIn.value!!)

        viewModel.onAuthenticated(mock(), AuthType.Existing)
        assertTrue(viewModel.signedIn.value!!)
    }

    @Test
    fun `observeAccountManager registers observer`() {
        val accountManager: FxaAccountManager = mockk(relaxed = true)
        val lifecycleOwner: LifecycleOwner = mockk()

        every { accountManager.authenticatedAccount() } returns null
        viewModel.observeAccountManager(accountManager, lifecycleOwner)

        verify { accountManager.register(viewModel, lifecycleOwner) }
        assertFalse(viewModel.signedIn.value!!)

        every { accountManager.authenticatedAccount() } returns mockk()
        viewModel.observeAccountManager(accountManager, lifecycleOwner)

        verify { accountManager.register(viewModel, lifecycleOwner) }
        assertTrue(viewModel.signedIn.value!!)
    }
}
