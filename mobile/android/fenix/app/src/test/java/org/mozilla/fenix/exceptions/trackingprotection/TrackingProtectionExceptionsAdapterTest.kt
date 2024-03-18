/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import android.content.Context
import android.widget.FrameLayout
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsAdapter
import org.mozilla.fenix.exceptions.viewholders.ExceptionsDeleteButtonViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsHeaderViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsListItemViewHolder
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionExceptionsAdapterTest {

    private lateinit var interactor: TrackingProtectionExceptionsInteractor
    private lateinit var adapter: TrackingProtectionExceptionsAdapter
    private lateinit var context: Context

    @Before
    fun setup() {
        interactor = mockk()
        adapter = TrackingProtectionExceptionsAdapter(interactor)
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
    }

    @Test
    fun `creates correct view holder type`() {
        every { testContext.components.core.icons } returns BrowserIcons(testContext, mockk(relaxed = true))
        val parent = FrameLayout(context)
        adapter.updateData(listOf(mockk(), mockk()))
        assertEquals(4, adapter.itemCount)

        val holders = (0 until adapter.itemCount).asSequence()
            .map { i -> adapter.getItemViewType(i) }
            .map { viewType -> adapter.onCreateViewHolder(parent, viewType) }
            .toList()
        assertEquals(4, holders.size)

        assertTrue(holders[0] is ExceptionsHeaderViewHolder)
        assertTrue(holders[1] is ExceptionsListItemViewHolder<*>)
        assertTrue(holders[2] is ExceptionsListItemViewHolder<*>)
        assertTrue(holders[3] is ExceptionsDeleteButtonViewHolder)
    }

    @Test
    fun `headers and delete should check if the other object is the same`() {
        assertTrue(
            TrackingProtectionExceptionsAdapter.DiffCallback.areItemsTheSame(
                ExceptionsAdapter.AdapterItem.Header,
                ExceptionsAdapter.AdapterItem.Header,
            ),
        )
        assertTrue(
            TrackingProtectionExceptionsAdapter.DiffCallback.areItemsTheSame(
                ExceptionsAdapter.AdapterItem.DeleteButton,
                ExceptionsAdapter.AdapterItem.DeleteButton,
            ),
        )
        assertFalse(
            TrackingProtectionExceptionsAdapter.DiffCallback.areItemsTheSame(
                ExceptionsAdapter.AdapterItem.Header,
                ExceptionsAdapter.AdapterItem.DeleteButton,
            ),
        )
        assertTrue(
            TrackingProtectionExceptionsAdapter.DiffCallback.areContentsTheSame(
                ExceptionsAdapter.AdapterItem.Header,
                ExceptionsAdapter.AdapterItem.Header,
            ),
        )
        assertTrue(
            TrackingProtectionExceptionsAdapter.DiffCallback.areContentsTheSame(
                ExceptionsAdapter.AdapterItem.DeleteButton,
                ExceptionsAdapter.AdapterItem.DeleteButton,
            ),
        )
        assertFalse(
            TrackingProtectionExceptionsAdapter.DiffCallback.areContentsTheSame(
                ExceptionsAdapter.AdapterItem.DeleteButton,
                ExceptionsAdapter.AdapterItem.Header,
            ),
        )
    }

    @Test
    fun `items with the same url should be marked as same`() {
        assertTrue(
            TrackingProtectionExceptionsAdapter.DiffCallback.areItemsTheSame(
                TrackingProtectionExceptionsAdapter.TrackingProtectionAdapterItem(
                    mockk {
                        every { url } returns "https://mozilla.org"
                    },
                ),
                TrackingProtectionExceptionsAdapter.TrackingProtectionAdapterItem(
                    mockk {
                        every { url } returns "https://mozilla.org"
                    },
                ),
            ),
        )
        assertFalse(
            TrackingProtectionExceptionsAdapter.DiffCallback.areItemsTheSame(
                TrackingProtectionExceptionsAdapter.TrackingProtectionAdapterItem(
                    mockk {
                        every { url } returns "https://mozilla.org"
                    },
                ),
                TrackingProtectionExceptionsAdapter.TrackingProtectionAdapterItem(
                    mockk {
                        every { url } returns "https://firefox.com"
                    },
                ),
            ),
        )
        assertFalse(
            TrackingProtectionExceptionsAdapter.DiffCallback.areItemsTheSame(
                TrackingProtectionExceptionsAdapter.TrackingProtectionAdapterItem(
                    mockk {
                        every { url } returns "https://mozilla.org"
                    },
                ),
                ExceptionsAdapter.AdapterItem.Header,
            ),
        )
        assertFalse(
            TrackingProtectionExceptionsAdapter.DiffCallback.areItemsTheSame(
                ExceptionsAdapter.AdapterItem.DeleteButton,
                TrackingProtectionExceptionsAdapter.TrackingProtectionAdapterItem(
                    mockk {
                        every { url } returns "https://mozilla.org"
                    },
                ),
            ),
        )
    }
}
