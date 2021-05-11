/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TabsTrayStore

@RunWith(FenixRobolectricTestRunner::class)
class BrowserTrayListTest {

    @Test
    fun `WHEN recyclerview detaches from window THEN notify adapter`() {
        val trayList = BrowserTrayList(testContext)
        val adapter = mockk<BrowserTabsAdapter>(relaxed = true)

        trayList.adapter = adapter
        trayList.tabsTrayStore = TabsTrayStore()

        trayList.onDetachedFromWindow()

        verify { adapter.onDetachedFromRecyclerView(trayList) }
    }
}
