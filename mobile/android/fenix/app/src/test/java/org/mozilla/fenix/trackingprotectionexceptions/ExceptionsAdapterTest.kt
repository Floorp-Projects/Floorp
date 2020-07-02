/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotectionexceptions

import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.trackingprotectionexceptions.viewholders.ExceptionsDeleteButtonViewHolder
import org.mozilla.fenix.trackingprotectionexceptions.viewholders.ExceptionsHeaderViewHolder
import org.mozilla.fenix.trackingprotectionexceptions.viewholders.ExceptionsListItemViewHolder

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class ExceptionsAdapterTest {

    private lateinit var interactor: ExceptionsInteractor
    private lateinit var adapter: ExceptionsAdapter

    @Before
    fun setup() {
        interactor = mockk()
        adapter = ExceptionsAdapter(interactor)
    }

    @Test
    fun `binds header and delete button with other adapter items`() = runBlockingTest {
        adapter.updateData(listOf(mockk(), mockk()))

        assertEquals(4, adapter.itemCount)
        assertEquals(ExceptionsHeaderViewHolder.LAYOUT_ID, adapter.getItemViewType(0))
        assertEquals(ExceptionsListItemViewHolder.LAYOUT_ID, adapter.getItemViewType(1))
        assertEquals(ExceptionsListItemViewHolder.LAYOUT_ID, adapter.getItemViewType(2))
        assertEquals(ExceptionsDeleteButtonViewHolder.LAYOUT_ID, adapter.getItemViewType(3))
    }
}
