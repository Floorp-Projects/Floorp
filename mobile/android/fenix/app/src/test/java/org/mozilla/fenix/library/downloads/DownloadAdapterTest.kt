/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import androidx.recyclerview.widget.RecyclerView
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DownloadAdapterTest {

    private lateinit var interactor: DownloadInteractor
    private lateinit var adapter: DownloadAdapter

    @Before
    fun setup() {
        interactor = mockk()
        adapter = DownloadAdapter(interactor)

        every { interactor.select(any()) } just Runs
    }

    @Test
    fun `getItemCount should return the number of tab collections`() {
        val download = mockk<DownloadItem>()

        assertEquals(0, adapter.itemCount)

        adapter.updateDownloads(
            downloads = listOf(download)
        )
        assertEquals(1, adapter.itemCount)
    }

    @Test
    fun `updateData inserts item`() {
        val download = mockk<DownloadItem> {
        }
        val observer = mockk<RecyclerView.AdapterDataObserver>(relaxed = true)
        adapter.registerAdapterDataObserver(observer)
        adapter.updateDownloads(
            downloads = listOf(download)
        )
        verify { observer.onChanged() }
    }
}
