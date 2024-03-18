/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.listadapters

import android.view.ViewGroup
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.share.ShareInteractor
import org.mozilla.fenix.share.viewholders.AppViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class AppShareAdapterTest {

    private val appOptions = mutableListOf(
        AppShareOption("App 0", mockk(), "package 0", "activity 0"),
        AppShareOption("App 1", mockk(), "package 1", "activity 1"),
    )
    private val appOptionsEmpty = emptyList<AppShareOption>()
    private val interactor: ShareInteractor = mockk(relaxed = true)

    @Test
    fun `updateData should call submitList()`() {
        // Used AppShareAdapter as a spy to ease testing of submitList()
        // and appOptionsEmpty to be able to record them being called
        val adapter = spyk(AppShareAdapter(mockk()).apply { submitList(appOptionsEmpty) })
        every { adapter.submitList(any()) } just Runs

        adapter.submitList(appOptions)

        verifyOrder {
            adapter.submitList(appOptions)
        }
    }

    @Test
    fun `getItemCount on a default instantiated Adapter should return 0`() {
        val adapter = AppShareAdapter(mockk())

        assertEquals(0, adapter.itemCount)
    }

    @Test
    fun `getItemCount after updateData() call should return the the passed in list's size`() {
        val adapter = AppShareAdapter(mockk()).apply { submitList(appOptions) }

        assertEquals(2, adapter.itemCount)
    }

    @Test
    fun `the adapter uses the right ViewHolder`() {
        val adapter = AppShareAdapter(interactor)
        val parentView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext

        val viewHolder = adapter.onCreateViewHolder(parentView, 0)

        assertEquals(AppViewHolder::class, viewHolder::class)
    }

    @Test
    fun `the adapter passes the Interactor to the ViewHolder`() {
        val adapter = AppShareAdapter(interactor)
        val parentView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext

        val viewHolder = adapter.onCreateViewHolder(parentView, 0)

        assertEquals(interactor, viewHolder.interactor)
    }

    @Test
    fun `the adapter binds the right item to a ViewHolder`() {
        val adapter = AppShareAdapter(interactor).apply { submitList(appOptions) }
        val parentView: ViewGroup = mockk(relaxed = true)
        val itemView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext
        every { itemView.context } returns testContext
        val viewHolder = spyk(AppViewHolder(parentView, mockk()))
        every { adapter.onCreateViewHolder(parentView, 0) } returns viewHolder
        every { viewHolder.bind(any()) } just Runs

        adapter.bindViewHolder(viewHolder, 1)

        verify { viewHolder.bind(appOptions[1]) }
    }
}
