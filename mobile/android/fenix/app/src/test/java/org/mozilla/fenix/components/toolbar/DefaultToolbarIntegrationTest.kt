/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DefaultToolbarIntegrationTest {
    private lateinit var feature: DefaultToolbarIntegration

    @Before
    fun setup() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        every { any<Context>().components } returns mockk {
            every { core } returns mockk {
                every { store } returns BrowserStore()
            }
            every { publicSuffixList } returns mockk()
            every { settings } returns mockk(relaxed = true)
        }

        feature = DefaultToolbarIntegration(
            context = testContext,
            toolbar = mockk(relaxed = true),
            toolbarMenu = mockk(relaxed = true),
            domainAutocompleteProvider = mockk(relaxed = true),
            historyStorage = mockk(),
            lifecycleOwner = mockk(),
            sessionId = null,
            isPrivate = false,
            interactor = mockk(),
            engine = mockk(),
        )
    }

    @After
    fun teardown() {
        unmockkStatic("org.mozilla.fenix.ext.ContextKt")
    }

    @Test
    fun `WHEN the feature starts THEN start the cfr presenter`() {
        feature.cfrPresenter = mockk(relaxed = true)

        feature.start()

        verify { feature.cfrPresenter.start() }
    }

    @Test
    fun `WHEN the feature stops THEN stop the cfr presenter`() {
        feature.cfrPresenter = mockk(relaxed = true)

        feature.stop()

        verify { feature.cfrPresenter.stop() }
    }
}
