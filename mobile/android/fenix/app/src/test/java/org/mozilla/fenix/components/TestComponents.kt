/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.mockk
import org.mozilla.fenix.helpers.perf.TestStrictModeManager
import org.mozilla.fenix.utils.ClipboardHandler
import org.mozilla.fenix.utils.Settings

class TestComponents(private val context: Context) : Components(context) {
    override val backgroundServices by lazy {
        mockk<BackgroundServices>(relaxed = true)
    }
    override val services by lazy { Services(context, backgroundServices.accountManager) }
    override val core by lazy { TestCore(context, analytics.crashReporter) }
    @Suppress("Deprecation")
    override val useCases by lazy {
        UseCases(
            context,
            core.engine,
            core.store,
            core.webAppShortcutManager,
            core.topSitesStorage,
            core.bookmarksStorage
        )
    }
    override val intentProcessors by lazy { mockk<IntentProcessors>(relaxed = true) }
    override val analytics by lazy { Analytics(context) }

    override val clipboardHandler by lazy { ClipboardHandler(context) }

    override val settings by lazy { mockk<Settings>(relaxed = true) }

    override val strictMode by lazy { TestStrictModeManager() }
}
