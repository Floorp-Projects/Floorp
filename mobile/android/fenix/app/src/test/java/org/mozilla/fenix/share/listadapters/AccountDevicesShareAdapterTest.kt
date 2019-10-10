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
import io.mockk.verifyOrder
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.share.ShareInteractor
import org.mozilla.fenix.share.viewholders.AccountDeviceViewHolder
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@UseExperimental(ObsoleteCoroutinesApi::class)
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class AccountDevicesShareAdapterTest {
    private val syncOptions = mutableListOf(SyncShareOption.AddNewDevice, SyncShareOption.SignIn)
    private val syncOptionsEmpty = mutableListOf<SyncShareOption>()
    private val interactor: ShareInteractor = mockk(relaxed = true)

    @Test
    fun `updateData should replace all previous data with argument and call notifyDataSetChanged()`() {
        // Used AccountDevicesShareAdapter as a spy to ease testing of notifyDataSetChanged()
        // and syncOptionsEmpty to be able to record them being called
        val adapter = spyk(AccountDevicesShareAdapter(mockk(), syncOptionsEmpty))
        every { adapter.notifyDataSetChanged() } just Runs

        adapter.updateData(syncOptions)

        verifyOrder {
            syncOptionsEmpty.clear()
            syncOptionsEmpty.addAll(syncOptions)
            adapter.notifyDataSetChanged()
        }
    }

    @Test
    fun `getItemCount on a default instantiated Adapter should return 0`() {
        val adapter = AccountDevicesShareAdapter(mockk())

        assertThat(adapter.itemCount).isEqualTo(0)
    }

    @Test
    fun `getItemCount after updateData() call should return the the passed in list's size`() {
        val adapter = AccountDevicesShareAdapter(mockk(), syncOptions)

        assertThat(adapter.itemCount).isEqualTo(2)
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
        val adapter = AccountDevicesShareAdapter(interactor, syncOptions)
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
