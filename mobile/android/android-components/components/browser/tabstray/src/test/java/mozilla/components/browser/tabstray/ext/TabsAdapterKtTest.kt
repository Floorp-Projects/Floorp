/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray.ext

import mozilla.components.browser.tabstray.TabsAdapter
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class TabsAdapterKtTest {

    @Test
    fun `doOnTabsUpdated is invoked once`() {
        val adapter = TabsAdapter()
        var invokedCount = 0

        adapter.doOnTabsUpdated {
            invokedCount++
        }

        assertEquals(0, invokedCount)

        adapter.notifyObservers { onTabsUpdated() }

        assertEquals(1, invokedCount)

        adapter.notifyObservers { onTabsUpdated() }

        assertEquals(1, invokedCount)
        assertFalse(adapter.isObserved())
    }
}
