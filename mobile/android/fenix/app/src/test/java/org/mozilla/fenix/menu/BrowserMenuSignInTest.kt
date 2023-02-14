/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.menu

import android.R
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import mozilla.components.service.fxa.store.Account
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.toolbar.BrowserMenuSignIn
import org.mozilla.fenix.ext.components

@RunWith(AndroidJUnit4::class)
class BrowserMenuSignInTest {
    private lateinit var context: Context
    private lateinit var components: Components
    private val account: Account = mockk {
        every { email } returns "bugzilla@mozilla.com"
    }

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        components = mockk(relaxed = true)

        every { context.resources } returns testContext.resources
        every { context.components } returns components
    }

    @Test
    fun `WHEN signed in and has profile data, THEN show email`() {
        every { components.backgroundServices.syncStore.state.account } returns account
        every { components.settings.signedInFxaAccount } returns true

        assertEquals(account.email, BrowserMenuSignIn(R.color.black).getLabel(context))
    }

    @Test
    fun `WHEN not signed in, THEN show the sync and save data text`() {
        every { components.settings.signedInFxaAccount } returns false
        every { components.backgroundServices.syncStore.state.account } returns null

        assertEquals(
            testContext.getString(org.mozilla.fenix.R.string.sync_menu_sync_and_save_data),
            BrowserMenuSignIn(R.color.black).getLabel(context),
        )
    }

    @Test
    fun `WHEN not signed in and has profile data, THEN show the sync and save data text`() {
        every { components.settings.signedInFxaAccount } returns false
        every { components.backgroundServices.syncStore.state.account } returns account

        assertEquals(
            testContext.getString(org.mozilla.fenix.R.string.sync_menu_sync_and_save_data),
            BrowserMenuSignIn(R.color.black).getLabel(context),
        )
    }

    @Test
    fun `WHEN signed in and has no profile data, THEN show the account info text`() {
        every { components.settings.signedInFxaAccount } returns true
        every { components.backgroundServices.syncStore.state.account } returns null

        assertEquals(
            testContext.getString(org.mozilla.fenix.R.string.browser_menu_account_settings),
            BrowserMenuSignIn(R.color.black).getLabel(context),
        )
    }
}
