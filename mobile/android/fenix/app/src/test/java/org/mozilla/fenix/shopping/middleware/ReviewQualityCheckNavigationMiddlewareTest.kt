package org.mozilla.fenix.shopping.middleware

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

class ReviewQualityCheckNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `WHEN opening an external link THEN the link should be opened in a new tab`() {
        var isOpenedInSelectedTab = false
        var isOpenedInNewTab = false
        val store = ReviewQualityCheckStore(
            middleware = listOf(
                ReviewQualityCheckNavigationMiddleware(
                    openLink = { _, openInNewTab ->
                        if (openInNewTab) {
                            isOpenedInNewTab = true
                        } else {
                            isOpenedInSelectedTab = true
                        }
                    },
                    scope = scope,
                ),
            ),
        )
        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        store.dispatch(ReviewQualityCheckAction.OpenLink(ReviewQualityCheckState.LinkType.ExternalLink("www.mozilla.com"))).joinBlocking()
        store.waitUntilIdle()

        assertEquals(true, isOpenedInNewTab)
        assertEquals(false, isOpenedInSelectedTab)
    }

    @Test
    fun `WHEN re-analzying a product THEN the link should be opened in the currently selected tab`() {
        var isOpenedInSelectedTab = false
        var isOpenedInNewTab = false
        val store = ReviewQualityCheckStore(
            middleware = listOf(
                ReviewQualityCheckNavigationMiddleware(
                    openLink = { _, openInNewTab ->
                        if (openInNewTab) {
                            isOpenedInNewTab = true
                        } else {
                            isOpenedInSelectedTab = true
                        }
                    },
                    scope = scope,
                ),
            ),
        )
        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        store.dispatch(ReviewQualityCheckAction.OpenLink(ReviewQualityCheckState.LinkType.AnalyzeLink("www.mozilla.com"))).joinBlocking()
        store.waitUntilIdle()

        assertEquals(true, isOpenedInSelectedTab)
        assertEquals(false, isOpenedInNewTab)
    }
}
