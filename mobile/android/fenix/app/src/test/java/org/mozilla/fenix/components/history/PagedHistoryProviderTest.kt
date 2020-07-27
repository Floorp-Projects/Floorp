/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.history

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import org.junit.Before
import org.junit.Test

class PagedHistoryProviderTest {

    private lateinit var storage: HistoryStorage

    @Before
    fun setup() {
        storage = mockk()
    }

    @Test
    fun `getHistory uses getVisitsPaginated`() {
        val provider = storage.createSynchronousPagedHistoryProvider()
        val results = listOf<VisitInfo>(mockk(), mockk())
        val onComplete = mockk<(List<VisitInfo>) -> Unit>(relaxed = true)

        coEvery { storage.getVisitsPaginated(any(), any(), any()) } returns results

        provider.getHistory(10L, 5, onComplete)

        coVerify {
            storage.getVisitsPaginated(
                offset = 10L,
                count = 5,
                excludeTypes = listOf(
                    VisitType.NOT_A_VISIT,
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                    VisitType.REDIRECT_PERMANENT
                )
            )
        }
        verify { onComplete(results) }
    }
}
