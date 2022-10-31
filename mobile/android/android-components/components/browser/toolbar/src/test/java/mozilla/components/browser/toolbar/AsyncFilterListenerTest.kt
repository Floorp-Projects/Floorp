/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.isActive
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.support.test.mock
import mozilla.components.ui.autocomplete.AutocompleteView
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import org.mockito.Mockito.atLeast
import org.mockito.Mockito.atLeastOnce
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.concurrent.Executor

@ExperimentalCoroutinesApi // for runTest
class AsyncFilterListenerTest {
    @Test
    fun `filter listener cancels prior filter executions`() = runTest {
        val urlView: AutocompleteView = mock()
        val filter: suspend (String, AutocompleteDelegate) -> Unit = mock()

        val dispatcher = spy(
            Executor {
                it.run()
            }.asCoroutineDispatcher(),
        )

        val listener = AsyncFilterListener(urlView, dispatcher, filter)

        verify(dispatcher, never()).cancelChildren()

        listener("test")

        verify(dispatcher, atLeastOnce()).cancelChildren()
    }

    @Test
    fun `filter delegate checks for cancellations before it runs, passes results to autocomplete view`() = runTest {
        var filter: suspend (String, AutocompleteDelegate) -> Unit = { query, delegate ->
            assertEquals("test", query)
            delegate.applyAutocompleteResult(
                AutocompleteResult(
                    input = "test",
                    text = "testing.com",
                    url = "http://www.testing.com",
                    source = "asyncTest",
                    totalItems = 1,
                ),
            )
        }

        val dispatcher = spy(
            Executor {
                it.run()
            }.asCoroutineDispatcher(),
        )

        var didCallApply = 0

        var listener = AsyncFilterListener(
            object : AutocompleteView {
                override val originalText: String = "test"

                override fun applyAutocompleteResult(result: InlineAutocompleteEditText.AutocompleteResult) {
                    assertEquals("asyncTest", result.source)
                    assertEquals("testing.com", result.text)
                    assertEquals(1, result.totalItems)
                    didCallApply += 1
                }

                override fun noAutocompleteResult() {
                    fail()
                }
            },
            dispatcher,
            filter,
            this.coroutineContext,
        )

        verify(dispatcher, never()).isActive

        async { listener("test") }.await()

        // Checked if parent scope is still active. Somehow, each access to 'isActive' registers as 4?
        verify(dispatcher, atLeast(4)).isActive
        // Passed the result to the view's apply method exactly once.
        assertEquals(1, didCallApply)

        filter = { query, delegate ->
            assertEquals("moz", query)
            delegate.applyAutocompleteResult(
                AutocompleteResult(
                    input = "moz",
                    text = "mozilla.com",
                    url = "http://www.mozilla.com",
                    source = "asyncTestTwo",
                    totalItems = 2,
                ),
            )
        }
        listener = AsyncFilterListener(
            object : AutocompleteView {
                override val originalText: String = "moz"

                override fun applyAutocompleteResult(result: InlineAutocompleteEditText.AutocompleteResult) {
                    assertEquals("asyncTestTwo", result.source)
                    assertEquals("mozilla.com", result.text)
                    assertEquals(2, result.totalItems)
                    didCallApply += 1
                }

                override fun noAutocompleteResult() {
                    fail()
                }
            },
            dispatcher,
            filter,
            this.coroutineContext,
        )

        async { listener("moz") }.await()

        verify(dispatcher, atLeast(8)).isActive
        assertEquals(2, didCallApply)
    }

    @Test
    fun `delegate discards stale results`() = runTest {
        val filter: suspend (String, AutocompleteDelegate) -> Unit = { query, delegate ->
            assertEquals("test", query)
            delegate.applyAutocompleteResult(
                AutocompleteResult(
                    input = "test",
                    text = "testing.com",
                    url = "http://www.testing.com",
                    source = "asyncTest",
                    totalItems = 1,
                ),
            )
        }

        val dispatcher = Executor {
            it.run()
        }.asCoroutineDispatcher()

        val listener = AsyncFilterListener(
            object : AutocompleteView {
                override val originalText: String = "nolongertest"

                override fun applyAutocompleteResult(result: InlineAutocompleteEditText.AutocompleteResult) {
                    fail()
                }

                override fun noAutocompleteResult() {
                    fail()
                }
            },
            dispatcher,
            filter,
            this.coroutineContext,
        )

        listener("test")
    }

    @Test
    fun `delegate discards stale lack of results`() = runTest {
        val filter: suspend (String, AutocompleteDelegate) -> Unit = { query, delegate ->
            assertEquals("test", query)
            delegate.noAutocompleteResult("test")
        }

        val dispatcher = Executor {
            it.run()
        }.asCoroutineDispatcher()

        val listener = AsyncFilterListener(
            object : AutocompleteView {
                override val originalText: String = "nolongertest"

                override fun applyAutocompleteResult(result: InlineAutocompleteEditText.AutocompleteResult) {
                    fail()
                }

                override fun noAutocompleteResult() {
                    fail()
                }
            },
            dispatcher,
            filter,
            this.coroutineContext,
        )

        listener("test")
    }

    @Test
    fun `delegate passes through non-stale lack of results`() = runTest {
        val filter: suspend (String, AutocompleteDelegate) -> Unit = { query, delegate ->
            assertEquals("test", query)
            delegate.noAutocompleteResult("test")
        }

        val dispatcher = Executor {
            it.run()
        }.asCoroutineDispatcher()

        var calledNoResults = 0
        val listener = AsyncFilterListener(
            object : AutocompleteView {
                override val originalText: String = "test"

                override fun applyAutocompleteResult(result: InlineAutocompleteEditText.AutocompleteResult) {
                    fail()
                }

                override fun noAutocompleteResult() {
                    calledNoResults += 1
                }
            },
            dispatcher,
            filter,
            this.coroutineContext,
        )

        async { listener("test") }.await()

        assertEquals(1, calledNoResults)
    }

    @Test
    fun `delegate discards results if parent scope was cancelled`() = runTest {
        var preservedDelegate: AutocompleteDelegate? = null

        val filter: suspend (String, AutocompleteDelegate) -> Unit = { query, delegate ->
            preservedDelegate = delegate
            assertEquals("test", query)
            delegate.applyAutocompleteResult(
                AutocompleteResult(
                    input = "test",
                    text = "testing.com",
                    url = "http://www.testing.com",
                    source = "asyncTest",
                    totalItems = 1,
                ),
            )
        }

        val dispatcher = Executor {
            it.run()
        }.asCoroutineDispatcher()

        var calledResults = 0
        val listener = AsyncFilterListener(
            object : AutocompleteView {
                override val originalText: String = "test"

                override fun applyAutocompleteResult(result: InlineAutocompleteEditText.AutocompleteResult) {
                    assertEquals("asyncTest", result.source)
                    assertEquals("testing.com", result.text)
                    assertEquals(1, result.totalItems)
                    calledResults += 1
                }

                override fun noAutocompleteResult() {
                    fail()
                }
            },
            dispatcher,
            filter,
            this.coroutineContext,
        )

        async {
            listener("test")
            listener("test")
        }.await()

        // This result application should be discarded, because scope has been cancelled by the second
        // 'listener' call above.
        preservedDelegate!!.applyAutocompleteResult(
            AutocompleteResult(
                input = "test",
                text = "testing.com",
                url = "http://www.testing.com",
                source = "asyncCancelled",
                totalItems = 1,
            ),
        )

        assertEquals(2, calledResults)
    }

    @Test
    fun `delegate discards lack of results if parent scope was cancelled`() = runTest {
        var preservedDelegate: AutocompleteDelegate? = null

        val filter: suspend (String, AutocompleteDelegate) -> Unit = { query, delegate ->
            preservedDelegate = delegate
            assertEquals("test", query)
            delegate.noAutocompleteResult("test")
        }

        val dispatcher = Executor {
            it.run()
        }.asCoroutineDispatcher()

        var calledResults = 0
        val listener = AsyncFilterListener(
            object : AutocompleteView {
                override val originalText: String = "test"

                override fun applyAutocompleteResult(result: InlineAutocompleteEditText.AutocompleteResult) {
                    fail()
                }

                override fun noAutocompleteResult() {
                    calledResults += 1
                }
            },
            dispatcher,
            filter,
            this.coroutineContext,
        )

        async {
            listener("test")
            listener("test")
        }.await()

        // This "no results" call should be discarded, because scope has been cancelled by the second
        // 'listener' call above.
        preservedDelegate!!.noAutocompleteResult("test")

        assertEquals(2, calledResults)
    }
}
