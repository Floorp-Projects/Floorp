/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import kotlinx.coroutines.experimental.CoroutineScope
import kotlinx.coroutines.experimental.delay
import kotlinx.coroutines.experimental.newSingleThreadContext
import kotlinx.coroutines.experimental.runBlocking
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.Shadows.shadowOf

@RunWith(RobolectricTestRunner::class)
class BrowserAwesomeBarTest {
    private val testMainScope = CoroutineScope(newSingleThreadContext("Test"))

    @Test
    fun `BrowserAwesomeBar forwards input to providers`() {
        runBlocking {
            val provider1 = mockProvider()
            val provider2 = mockProvider()
            val provider3 = mockProvider()

            val awesomeBar = BrowserAwesomeBar(RuntimeEnvironment.application)
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

        val awesomeBar = BrowserAwesomeBar(RuntimeEnvironment.application)
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

        val awesomeBar = BrowserAwesomeBar(RuntimeEnvironment.application)
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
        runBlocking {
            var providerTriggered = false
            var providerCancelled = false

            val blockingProvider = object : AwesomeBar.SuggestionProvider {
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

            val awesomeBar = BrowserAwesomeBar(RuntimeEnvironment.application)
            awesomeBar.scope = testMainScope
            awesomeBar.addProviders(blockingProvider)

            awesomeBar.onInputChanged("Hello!")

            // Give the jobs some time to start
            runBlocking { delay(50) }

            awesomeBar.onInputCancelled()

            // Wait for all jobs to have received the stop signal
            awesomeBar.job!!.join()

            assertTrue(providerTriggered)
            assertTrue(providerCancelled)
        }
    }

    @Test
    fun `BrowserAwesomeBar stops jobs when getting detached`() {
        runBlocking {
            var providerTriggered = false
            var providerCancelled = false

            val blockingProvider = object : AwesomeBar.SuggestionProvider {
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

            val awesomeBar = BrowserAwesomeBar(RuntimeEnvironment.application)
            awesomeBar.scope = testMainScope
            awesomeBar.addProviders(blockingProvider)

            awesomeBar.onInputChanged("Hello!")

            // Give the jobs some time to start
            runBlocking { delay(50) }

            shadowOf(awesomeBar).callOnDetachedFromWindow()

            // Wait for all jobs to have received the stop signal
            awesomeBar.job!!.join()

            assertTrue(providerTriggered)
            assertTrue(providerCancelled)
        }
    }

    @Test
    fun `BrowserAwesomeBar cancels previous jobs if onInputStarted gets called again`() {
        runBlocking {
            var firstProviderCallCancelled = false
            var timesProviderCalled = 0

            val provider = object : AwesomeBar.SuggestionProvider {
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

            val awesomeBar = BrowserAwesomeBar(RuntimeEnvironment.application)
            awesomeBar.scope = testMainScope
            awesomeBar.addProviders(provider)

            awesomeBar.onInputChanged("Hello!")

            // Give the jobs some time to start
            runBlocking { delay(50) }

            awesomeBar.onInputChanged("World!")

            awesomeBar.job!!.join()

            assertTrue(firstProviderCallCancelled)
            assertEquals(2, timesProviderCalled)
        }
    }

    @Test
    fun `onStopListener is accessible internally`() {
        var stopped = false

        val awesomeBar = BrowserAwesomeBar(RuntimeEnvironment.application)
        awesomeBar.setOnStopListener {
            stopped = true
        }

        awesomeBar.listener!!.invoke()

        assertTrue(stopped)
    }

    private fun mockProvider(): AwesomeBar.SuggestionProvider = spy(object : AwesomeBar.SuggestionProvider {
        override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
            return emptyList()
        }
    })
}
