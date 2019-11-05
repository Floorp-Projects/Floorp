/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.listadapters

import android.view.ViewGroup
import assertk.assertThat
import assertk.assertions.isEqualTo
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.share.ShareInteractor
import org.mozilla.fenix.share.viewholders.AccountDeviceViewHolder
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class AccountDevicesShareAdapterTest {
    private val interactor: ShareInteractor = mockk(relaxed = true)

    @Test
    fun `getItemCount on a default instantiated Adapter should return 0`() {
        val adapter = AccountDevicesShareAdapter(mockk())

        assertThat(adapter.itemCount).isEqualTo(0)
    }

    @Test
    fun `the adapter uses the right ViewHolder`() {
        val adapter = AccountDevicesShareAdapter(interactor)
        val parentView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext

        val viewHolder = adapter.onCreateViewHolder(parentView, 0)

        assertThat(viewHolder::class).isEqualTo(AccountDeviceViewHolder::class)
    }

    @Test
    fun `the adapter passes the Interactor to the ViewHolder`() {
        val adapter = AccountDevicesShareAdapter(interactor)
        val parentView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext

        val viewHolder = adapter.onCreateViewHolder(parentView, 0)

        assertThat(viewHolder.interactor).isEqualTo(interactor)
    }

    @Test
    fun `the adapter binds the right item to a ViewHolder`() {
        val syncOptions = listOf(SyncShareOption.AddNewDevice, SyncShareOption.SignIn)
        val adapter = AccountDevicesShareAdapter(interactor)
        adapter.submitList(syncOptions)
        val parentView: ViewGroup = mockk(relaxed = true)
        val itemView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext
        every { itemView.context } returns testContext
        val viewHolder = spyk(AccountDeviceViewHolder(parentView, mockk()))
        every { adapter.onCreateViewHolder(parentView, 0) } returns viewHolder
        every { viewHolder.bind(any()) } just Runs

        adapter.bindViewHolder(viewHolder, 1)

        verify { viewHolder.bind(syncOptions[1]) }
    }
}
