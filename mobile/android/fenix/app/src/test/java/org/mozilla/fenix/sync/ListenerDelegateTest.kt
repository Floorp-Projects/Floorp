/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class ListenerDelegateTest {
    @Test
    fun `delegate invokes nullable listener`() {
        val listener: SyncedTabsView.Listener? = mockk(relaxed = true)
        val metrics: MetricController = mockk(relaxed = true)

        val delegate = ListenerDelegate(metrics) { listener }

        delegate.onRefresh()

        verify { listener?.onRefresh() }

        delegate.onTabClicked(mockk())

        verify { listener?.onTabClicked(any()) }
        verify { metrics.track(Event.SyncedTabOpened) }
    }
}
