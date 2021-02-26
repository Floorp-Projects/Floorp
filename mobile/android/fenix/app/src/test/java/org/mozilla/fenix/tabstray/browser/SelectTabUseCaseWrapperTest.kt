/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class SelectTabUseCaseWrapperTest {

    val metricController = mockk<MetricController>(relaxed = true)
    val selectUseCase: TabsUseCases.SelectTabUseCase = mockk(relaxed = true)

    @Test
    fun `WHEN invoked THEN metrics, use case and callback are triggered`() {
        val onSelect: (String) -> Unit = mockk(relaxed = true)
        val wrapper = SelectTabUseCaseWrapper(metricController, selectUseCase, onSelect)

        wrapper("123")

        verify { metricController.track(Event.OpenedExistingTab) }
        verify { selectUseCase("123") }
        verify { onSelect("123") }
    }
}
