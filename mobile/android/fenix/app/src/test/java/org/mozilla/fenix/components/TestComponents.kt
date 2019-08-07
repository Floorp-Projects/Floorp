/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.mockk
import kotlinx.coroutines.ObsoleteCoroutinesApi

@ObsoleteCoroutinesApi
class TestComponents(private val context: Context) : Components(context) {
    override val backgroundServices by lazy {
        mockk<BackgroundServices>(relaxed = true)
    }
    override val services by lazy { Services(backgroundServices.accountManager) }
    override val core by lazy { TestCore(context) }
    override val search by lazy { Search(context) }
    override val useCases by lazy { UseCases(context, core.sessionManager, core.engine.settings, search.searchEngineManager) }
    override val utils by lazy {
        Utilities(
            context,
            core.sessionManager,
            useCases.sessionUseCases,
            useCases.searchUseCases
        )
    }
    override val analytics by lazy { Analytics(context) }
}
