/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class SelectTabUseCaseWrapper(
    private val metrics: MetricController,
    private val selectTab: TabsUseCases.SelectTabUseCase,
    private val onSelect: (String) -> Unit
) : TabsUseCases.SelectTabUseCase {
    override fun invoke(tabId: String) {
        metrics.track(Event.OpenedExistingTab)
        selectTab(tabId)
        onSelect(tabId)
    }
}

class RemoveTabUseCaseWrapper(
    private val metrics: MetricController,
    private val onRemove: (String) -> Unit
) : TabsUseCases.RemoveTabUseCase {
    override fun invoke(tabId: String) {
        metrics.track(Event.ClosedExistingTab)
        onRemove(tabId)
    }
}
