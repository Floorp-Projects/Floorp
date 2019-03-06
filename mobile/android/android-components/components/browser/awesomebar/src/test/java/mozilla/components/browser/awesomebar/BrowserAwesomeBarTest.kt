/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.awesomebar.transform.SuggestionTransformer
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.never
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
class BrowserAwesomeBarTest {
    private val testMainScope = CoroutineScope(newSingleThreadContext("Test"))

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `BrowserAwesomeBar forwards input to providers`() {
        runBlocking(testMainScope.coroutineContext) {
            val provider1 = mockProvider()
            val provider2 = mockProvider()
            val provider3 = mockProvider()

            val awesomeBar = BrowserAwesomeBar(context)
            awesomeBar.scope = testMainScope
            awesomeBar.addProviders(provider1, provider2)
            awesomeBar.addProviders(provider3)

            awesomeBar.onInputChanged("Hello World!")

            awesomeBar.job!!.join()

            verify(provider1).onInputChanged("Hello World!")
            verify(provider2).onInputChanged("Hello World!")
            verify(provider3).onInputChanged("Hello World!")
        }
    }

    @Test
    fun `BrowserAwesomeBar forwards onInputStarted to providers`() {
        val provider1: AwesomeBar.SuggestionProvider = mock()
        val provider2: AwesomeBar.SuggestionProvider = mock()
        val provider3: AwesomeBar.SuggestionProvider = mock()

        val awesomeBar = BrowserAwesomeBar(context)
        awesomeBar.addProviders(provider1, provider2)
        awesomeBar.addProviders(provider3)

        awesomeBar.onInputStarted()

        verify(provider1).onInputStarted()
        verify(provider2).onInputStarted()
        verify(provider3).onInputStarted()
    }

    @Test
    fun `BrowserAwesomeBar forwards onInputCancelled to providers`() {
        val provider1: AwesomeBar.SuggestionProvider = mock()
        val provider2: AwesomeBar.SuggestionProvider = mock()
        val provider3: AwesomeBar.SuggestionProvider = mock()

        val awesomeBar = BrowserAwesomeBar(context)
        awesomeBar.addProviders(provider1, provider2)
        awesomeBar.addProviders(provider3)

        awesomeBar.onInputCancelled()

        verify(provider1).onInputCancelled()
        verify(provider2).onInputCancelled()
        verify(provider3).onInputCancelled()

        verifyNoMoreInteractions(provider1)
        verifyNoMoreInteractions(provider2)
        verifyNoMoreInteractions(provider3)
    }

    @Test
    fun `onInputCancelled stops jobs`() {
        runBlocking(testMainScope.coroutineContext) {
            var providerTriggered = false
            var providerCancelled = false

            val blockingProvider = object : AwesomeBar.SuggestionProvider {
                override val id: String = UUID.randomUUID().toString()

                override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
                    providerTriggered = true

                    try {
                        // We can only escape this by cancelling the coroutine
                        while (true) {
                            delay(10)
                        }
                    } finally {
                        providerCancelled = true
                    }
                }
            }

            val awesomeBar = BrowserAwesomeBar(context)
            awesomeBar.scope = testMainScope
            awesomeBar.addProviders(blockingProvider)

            awesomeBar.onInputChanged("Hello!")

            // Give the jobs some time to start
            delay(50)

            awesomeBar.onInputCancelled()

            // Wait for all jobs to have received the stop signal
            awesomeBar.job!!.join()

            assertTrue(providerTriggered)
            assertTrue(providerCancelled)
        }
    }

    @Test
    fun `removeProvider removes the provider`() {
        runBlocking(testMainScope.coroutineContext) {
            val provider1 = mockProvider()
            val provider2 = mockProvider()
            val provider3 = mockProvider()

            val awesomeBar = BrowserAwesomeBar(context)
            val adapter: SuggestionsAdapter = mock()
            awesomeBar.suggestionsAdapter = adapter

            awesomeBar.addProviders(provider1, provider2)
            awesomeBar.removeProviders(provider2)
            awesomeBar.addProviders(provider3)

            awesomeBar.onInputStarted()

            // Confirm that only provider2's suggestions were removed
            verify(adapter, never()).removeSuggestions(provider1)
            verify(adapter).removeSuggestions(provider2)
            verify(adapter, never()).removeSuggestions(provider1)

            verify(provider1).onInputStarted()
            verify(provider2, never()).onInputStarted()
            verify(provider3).onInputStarted()
        }
    }

    @Test
    fun `removeAllProviders removes all providers`() {
        runBlocking(testMainScope.coroutineContext) {
            val provider1 = mockProvider()
            val provider2 = mockProvider()

            val awesomeBar = BrowserAwesomeBar(context)
            awesomeBar.addProviders(provider1, provider2)
            awesomeBar.removeAllProviders()

            awesomeBar.onInputStarted()

            verify(provider1, never()).onInputStarted()
            verify(provider2, never()).onInputStarted()
        }
    }

    @Test
    fun `BrowserAwesomeBar stops jobs when getting detached`() {
        runBlocking(testMainScope.coroutineContext) {
            var providerTriggered = false
            var providerCancelled = false

            val blockingProvider = object : AwesomeBar.SuggestionProvider {
                override val id: String = UUID.randomUUID().toString()

                override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
                    providerTriggered = true

                    try {
                        // We can only escape this by cancelling the coroutine
                        while (true) {
                            delay(10)
                        }
                    } finally {
                        providerCancelled = true
                    }
                }
            }

            val awesomeBar = BrowserAwesomeBar(context)
            awesomeBar.scope = testMainScope
            awesomeBar.addProviders(blockingProvider)

            awesomeBar.onInputChanged("Hello!")

            // Give the jobs some time to start
            delay(50)

            shadowOf(awesomeBar).callOnDetachedFromWindow()

            // Wait for all jobs to have received the stop signal
            awesomeBar.job!!.join()

            assertTrue(providerTriggered)
            assertTrue(providerCancelled)
        }
    }

    @Test
    fun `BrowserAwesomeBar cancels previous jobs if onInputStarted gets called again`() {
        runBlocking(testMainScope.coroutineContext) {
            var firstProviderCallCancelled = false
            var timesProviderCalled = 0

            val provider = object : AwesomeBar.SuggestionProvider {
                override val id: String = UUID.randomUUID().toString()

                var isFirstCall = true

                override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
                    println("Provider called with: $text")

                    timesProviderCalled++

                    // Our first call is blocking indefinitely and should get cancelled by the second
                    // call that just passes.

                    if (!isFirstCall) {
                        return emptyList()
                    }

                    isFirstCall = false

                    try {
                        // We can only escape this by cancelling the coroutine
                        while (true) {
                            delay(10)
                        }
                    } finally {
                        firstProviderCallCancelled = true
                    }
                }
            }

            val awesomeBar = BrowserAwesomeBar(context)
            awesomeBar.scope = testMainScope
            awesomeBar.addProviders(provider)

            awesomeBar.onInputChanged("Hello!")

            // Give the jobs some time to start
            delay(50)

            awesomeBar.onInputChanged("World!")

            awesomeBar.job!!.join()

            assertTrue(firstProviderCallCancelled)
            assertEquals(2, timesProviderCalled)
        }
    }

    @Test
    fun `BrowserAwesomeBar will use optional transformer before passing suggestions to adapter`() {
        runBlocking(testMainScope.coroutineContext) {
            val awesomeBar = BrowserAwesomeBar(context)
            awesomeBar.scope = testMainScope

            val inputSuggestions = listOf(AwesomeBar.Suggestion(mock(), title = "Tetst"))
            val provider = object : AwesomeBar.SuggestionProvider {
                override val id: String = UUID.randomUUID().toString()

                override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
                    return inputSuggestions
                }
            }

            awesomeBar.addProviders(provider)

            val adapter: SuggestionsAdapter = mock()
            awesomeBar.suggestionsAdapter = adapter

            val transformedSuggestions = listOf(
                AwesomeBar.Suggestion(provider, title = "Hello"),
                AwesomeBar.Suggestion(provider, title = "World")
            )

            val transformer = spy(object : SuggestionTransformer {
                override fun transform(
                    provider: AwesomeBar.SuggestionProvider,
                    suggestions: List<AwesomeBar.Suggestion>
                ): List<AwesomeBar.Suggestion> {
                    return transformedSuggestions
                }
            })
            awesomeBar.transformer = transformer

            awesomeBar.onInputChanged("Hello!")

            // Give the jobs some time to start
            delay(50)

            awesomeBar.job!!.start()
            awesomeBar.job!!.join()

            verify(transformer).transform(provider, inputSuggestions)
            verify(adapter).addSuggestions(provider, transformedSuggestions)
        }
    }

    @Test
    fun `onStopListener is accessible internally`() {
        var stopped = false

        val awesomeBar = BrowserAwesomeBar(context)
        awesomeBar.setOnStopListener {
            stopped = true
        }

        awesomeBar.listener!!.invoke()

        assertTrue(stopped)
    }

    private fun mockProvider(): AwesomeBar.SuggestionProvider = spy(object : AwesomeBar.SuggestionProvider {
        override val id: String = UUID.randomUUID().toString()

        override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
            return emptyList()
        }
    })
}
