/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.GleanMetrics.TabsTray

@Suppress("UndocumentedPublicClass")
class RemoveTabUseCaseWrapper(
    private val onRemove: (String) -> Unit,
) : TabsUseCases.RemoveTabUseCase {
    @Suppress("UndocumentedPublicFunction")
    operator fun invoke(tabId: String, source: String? = null) {
        TabsTray.closedExistingTab.record(TabsTray.ClosedExistingTabExtra(source ?: "unknown"))
        onRemove(tabId)
    }

    @Suppress("UndocumentedPublicFunction")
    override fun invoke(tabId: String) {
        invoke(tabId, null)
    }
}
