/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

class ReviewQualityCheckNavigationMiddlewareTest {

    private val sumoUrl = "https://support.mozilla.org/en-US/products/mobile"
    private lateinit var store: ReviewQualityCheckStore
    private lateinit var browserStore: BrowserStore
    private lateinit var addTabUseCase: TabsUseCases.SelectOrAddUseCase
    private lateinit var middleware: ReviewQualityCheckNavigationMiddleware

    @Before
    fun setup() {
        browserStore = BrowserStore()
        addTabUseCase = mockk(relaxed = true)
        middleware = ReviewQualityCheckNavigationMiddleware(
            selectOrAddUseCase = addTabUseCase,
            getReviewQualityCheckSumoUrl = mockk {
                every { this@mockk.invoke() } returns sumoUrl
            },
        )
        store = ReviewQualityCheckStore(
            middleware = listOf(middleware),
        )
    }

    @Test
    fun `WHEN opening an external link THEN the link should be opened in a new tab`() {
        val action = ReviewQualityCheckAction.OpenExplainerLearnMoreLink
        store.waitUntilIdle()
        assertEquals(0, browserStore.state.tabs.size)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        verify {
            addTabUseCase.invoke(sumoUrl)
        }
    }
}
