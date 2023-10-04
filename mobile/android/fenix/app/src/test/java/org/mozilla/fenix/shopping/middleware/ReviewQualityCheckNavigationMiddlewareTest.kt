package org.mozilla.fenix.shopping.middleware

import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

@RunWith(FenixRobolectricTestRunner::class)
class ReviewQualityCheckNavigationMiddlewareTest {

    private lateinit var store: ReviewQualityCheckStore
    private lateinit var browserStore: BrowserStore
    private lateinit var addTabUseCase: TabsUseCases.SelectOrAddUseCase
    private lateinit var middleware: ReviewQualityCheckNavigationMiddleware

    @Before
    fun setup() {
        browserStore = BrowserStore()
        addTabUseCase = TabsUseCases.SelectOrAddUseCase(browserStore)
        middleware = ReviewQualityCheckNavigationMiddleware(
            selectOrAddUseCase = addTabUseCase,
            context = testContext,
        )
        store = ReviewQualityCheckStore(
            middleware = listOf(middleware),
        )
    }

    @Test
    fun `WHEN opening an external link THEN the link should be opened in a new tab`() {
        val action = ReviewQualityCheckAction.OpenPoweredByLink
        store.waitUntilIdle()
        assertEquals(0, browserStore.state.tabs.size)

        store.dispatch(action).joinBlocking()
        store.waitUntilIdle()

        assertEquals(1, browserStore.state.tabs.size)
    }
}
