/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions

import android.widget.LinearLayout
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.every
import io.mockk.mockk
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.loginexceptions.viewholders.LoginExceptionsDeleteButtonViewHolder
import org.mozilla.fenix.loginexceptions.viewholders.LoginExceptionsHeaderViewHolder
import org.mozilla.fenix.loginexceptions.viewholders.LoginExceptionsListItemViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class LoginExceptionsAdapterTest {

    private lateinit var interactor: LoginExceptionsInteractor
    private lateinit var adapter: LoginExceptionsAdapter

    @Before
    fun setup() {
        interactor = mockk()
        adapter = LoginExceptionsAdapter(interactor)
    }

    @Test
    fun `creates correct view holder type`() {
        val parent = LinearLayout(ContextThemeWrapper(testContext, R.style.NormalTheme))
        adapter.updateData(listOf(mockk(), mockk()))
        assertEquals(4, adapter.itemCount)

        val holders = (0 until adapter.itemCount).asSequence()
            .map { i -> adapter.getItemViewType(i) }
            .map { viewType -> adapter.onCreateViewHolder(parent, viewType) }
            .toList()
        assertEquals(4, holders.size)

        assertTrue(holders[0] is LoginExceptionsHeaderViewHolder)
        assertTrue(holders[1] is LoginExceptionsListItemViewHolder)
        assertTrue(holders[2] is LoginExceptionsListItemViewHolder)
        assertTrue(holders[3] is LoginExceptionsDeleteButtonViewHolder)
    }

    @Test
    fun `headers and delete should check if the other object is the same`() {
        assertTrue(LoginExceptionsAdapter.DiffCallback.areItemsTheSame(
            LoginExceptionsAdapter.AdapterItem.Header,
            LoginExceptionsAdapter.AdapterItem.Header
        ))
        assertTrue(LoginExceptionsAdapter.DiffCallback.areItemsTheSame(
            LoginExceptionsAdapter.AdapterItem.DeleteButton,
            LoginExceptionsAdapter.AdapterItem.DeleteButton
        ))
        assertFalse(LoginExceptionsAdapter.DiffCallback.areItemsTheSame(
            LoginExceptionsAdapter.AdapterItem.Header,
            LoginExceptionsAdapter.AdapterItem.DeleteButton
        ))
        assertTrue(LoginExceptionsAdapter.DiffCallback.areContentsTheSame(
            LoginExceptionsAdapter.AdapterItem.Header,
            LoginExceptionsAdapter.AdapterItem.Header
        ))
        assertTrue(LoginExceptionsAdapter.DiffCallback.areContentsTheSame(
            LoginExceptionsAdapter.AdapterItem.DeleteButton,
            LoginExceptionsAdapter.AdapterItem.DeleteButton
        ))
        assertFalse(LoginExceptionsAdapter.DiffCallback.areContentsTheSame(
            LoginExceptionsAdapter.AdapterItem.DeleteButton,
            LoginExceptionsAdapter.AdapterItem.Header
        ))
    }

    @Test
    fun `items with the same id should be marked as same`() {
        assertTrue(LoginExceptionsAdapter.DiffCallback.areItemsTheSame(
            LoginExceptionsAdapter.AdapterItem.Item(mockk {
                every { id } returns 12L
            }),
            LoginExceptionsAdapter.AdapterItem.Item(mockk {
                every { id } returns 12L
            })
        ))
        assertFalse(LoginExceptionsAdapter.DiffCallback.areItemsTheSame(
            LoginExceptionsAdapter.AdapterItem.Item(mockk {
                every { id } returns 14L
            }),
            LoginExceptionsAdapter.AdapterItem.Item(mockk {
                every { id } returns 12L
            })
        ))
        assertFalse(LoginExceptionsAdapter.DiffCallback.areItemsTheSame(
            LoginExceptionsAdapter.AdapterItem.Item(mockk {
                every { id } returns 14L
            }),
            LoginExceptionsAdapter.AdapterItem.Header
        ))
        assertFalse(LoginExceptionsAdapter.DiffCallback.areItemsTheSame(
            LoginExceptionsAdapter.AdapterItem.DeleteButton,
            LoginExceptionsAdapter.AdapterItem.Item(mockk {
                every { id } returns 14L
            })
        ))
    }
}
