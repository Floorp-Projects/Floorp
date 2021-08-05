/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.awesomebar.facts.BrowserAwesomeBarFacts
import mozilla.components.browser.awesomebar.transform.SuggestionTransformer
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.robolectric.Shadows.shadowOf
import java.util.UUID

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class BrowserAwesomeBarTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `BrowserAwesomeBar forwards input to providers`() {
        runBlocking {
            val awesomeBar = BrowserAwesomeBar(testContext)

            val provider1 = mockProvider()
            val provider2 = mockProvider()
            val provider3 = mockProvider()

            awesomeBar.addProviders(provider1)

            val facts = mutableListOf<Fact>()
            Facts.registerProcessor(object : FactProcessor {
                override fun process(fact: Fact) {
                    facts.add(fact)
                }
            })

            awesomeBar.onInputChanged("Hello World!")
            awesomeBar.awaitForAllJobsToFinish()

            verify(provider1).onInputChanged("Hello World!")
            verifyNoInteractions(provider2)
            verifyNoInteractions(provider3)

            assertEquals(1, facts.size)
            assertBrowserAwesomebarFact(facts[0], provider1)

            awesomeBar.addProviders(provider2, provider3)

            awesomeBar.onInputChanged("Hello Back!")
            awesomeBar.awaitForAllJobsToFinish()

            verify(provider2).onInputChanged("Hello Back!")
            verify(provider3).onInputChanged("Hello Back!")

            assertEquals(4, facts.size)

            facts.forEach { assertBrowserAwesomebarFact(it) }
        }
    }

    private fun assertBrowserAwesomebarFact(f: Fact, provider: AwesomeBar.SuggestionProvider? = null) {
        assertEquals(Component.BROWSER_AWESOMEBAR, f.component)
        assertEquals(Action.INTERACTION, f.action)
        assertEquals(BrowserAwesomeBarFacts.Items.PROVIDER_DURATION, f.item)
        val pair = f.metadata!![BrowserAwesomeBarFacts.MetadataKeys.DURATION_PAIR] as Pair<*, *>
        provider?.let { assertEquals(pair.first, provider) }
        assertTrue(pair.second is Long)
    }

    @Test
    fun `BrowserAwesomeBar forwards onInputStarted to providers`() {
        val provider1: AwesomeBar.SuggestionProvider = mock()
        `when`(provider1.id).thenReturn("1")
        val provider2: AwesomeBar.SuggestionProvider = mock()
        `when`(provider2.id).thenReturn("2")
        val provider3: AwesomeBar.SuggestionProvider = mock()
        `when`(provider3.id).thenReturn("3")

        val awesomeBar = BrowserAwesomeBar(testContext)
        awesomeBar.addProviders(provider1, provider2)
        awesomeBar.addProviders(provider3)

        awesomeBar.onInputStarted()
        awesomeBar.awaitForAllJobsToFinish()

        verify(provider1).onInputStarted()
        verify(provider2).onInputStarted()
        verify(provider3).onInputStarted()
    }

    @Test
    fun `BrowserAwesomeBar forwards onInputCancelled to providers`() {
        val provider1: AwesomeBar.SuggestionProvider = mock()
        `when`(provider1.id).thenReturn("1")
        val provider2: AwesomeBar.SuggestionProvider = mock()
        `when`(provider2.id).thenReturn("2")
        val provider3: AwesomeBar.SuggestionProvider = mock()
        `when`(provider3.id).thenReturn("3")

        val awesomeBar = BrowserAwesomeBar(testContext)
        awesomeBar.addProviders(provider1, provider2)
        awesomeBar.addProviders(provider3)

        awesomeBar.onInputCancelled()

        verify(provider1).onInputCancelled()
        verify(provider2).onInputCancelled()
        verify(provider3).onInputCancelled()
    }

    @Test
    fun `onInputCancelled stops jobs`() {
        runBlocking {
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

            val awesomeBar = BrowserAwesomeBar(testContext)
            awesomeBar.addProviders(blockingProvider)

            awesomeBar.onInputChanged("Hello!")
            awaitFor { providerTriggered }

            awesomeBar.onInputCancelled()
            awesomeBar.awaitForAllJobsToFinish()

            assertTrue(providerTriggered)
            assertTrue(providerCancelled)
        }
    }

    @Test
    fun `removeProvider removes the provider`() {
        runBlocking {
            val provider1 = mockProvider()
            val provider2 = mockProvider()
            val provider3 = mockProvider()

            val awesomeBar = BrowserAwesomeBar(testContext)
            val adapter: SuggestionsAdapter = mock()
            awesomeBar.suggestionsAdapter = adapter

            assertEquals(PROVIDER_MAX_SUGGESTIONS * INITIAL_NUMBER_OF_PROVIDERS, awesomeBar.uniqueSuggestionIds.maxSize())
            awesomeBar.addProviders(provider1, provider2)
            assertEquals((PROVIDER_MAX_SUGGESTIONS * 2) * 2, awesomeBar.uniqueSuggestionIds.maxSize())
            awesomeBar.removeProviders(provider2)
            assertEquals((PROVIDER_MAX_SUGGESTIONS * 1) * 2, awesomeBar.uniqueSuggestionIds.maxSize())
            awesomeBar.addProviders(provider3)
            assertEquals((PROVIDER_MAX_SUGGESTIONS * 2) * 2, awesomeBar.uniqueSuggestionIds.maxSize())

            awesomeBar.onInputStarted()
            awesomeBar.awaitForAllJobsToFinish()

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
        runBlocking {
            val provider1 = mockProvider()
            val provider2 = mockProvider()

            val awesomeBar = BrowserAwesomeBar(testContext)
            assertEquals(PROVIDER_MAX_SUGGESTIONS * INITIAL_NUMBER_OF_PROVIDERS, awesomeBar.uniqueSuggestionIds.maxSize())
            awesomeBar.addProviders(provider1, provider2)
            assertEquals((PROVIDER_MAX_SUGGESTIONS * 2) * 2, awesomeBar.uniqueSuggestionIds.maxSize())

            // Verify that all cached suggestion IDs are evicted when all providers are removed
            awesomeBar.uniqueSuggestionIds.put("test", 1)
            awesomeBar.removeAllProviders()
            assertEquals(0, awesomeBar.uniqueSuggestionIds.size())

            awesomeBar.onInputStarted()

            verify(provider1, never()).onInputStarted()
            verify(provider2, never()).onInputStarted()
        }
    }

    @Test
    fun `BrowserAwesomeBar stops jobs when getting detached`() {
        runBlocking {
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

            val awesomeBar = BrowserAwesomeBar(testContext)
            awesomeBar.addProviders(blockingProvider)

            awesomeBar.onInputChanged("Hello!")
            awaitFor { providerTriggered }

            shadowOf(awesomeBar).callOnDetachedFromWindow()
            awesomeBar.awaitForAllJobsToFinish()

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

            val awesomeBar = BrowserAwesomeBar(testContext)
            awesomeBar.addProviders(provider)

            awesomeBar.onInputChanged("Hello!")
            awaitFor { timesProviderCalled > 0 }

            awesomeBar.onInputChanged("World!")
            awaitFor { firstProviderCallCancelled }
            awesomeBar.awaitForAllJobsToFinish()

            assertTrue(firstProviderCallCancelled)
            assertEquals(2, timesProviderCalled)
        }
    }

    @Test
    fun `BrowserAwesomeBar will use optional transformer before passing suggestions to adapter`() {
        runBlocking {
            val awesomeBar = BrowserAwesomeBar(testContext)

            val inputSuggestions = listOf(AwesomeBar.Suggestion(mock(), title = "Test"))
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
            awesomeBar.awaitForAllJobsToFinish()

            verify(transformer).transform(provider, inputSuggestions)
            verify(adapter).addSuggestions(provider, transformedSuggestions)
        }
    }

    @Test
    fun `onStopListener is accessible internally`() {
        var stopped = false

        val awesomeBar = BrowserAwesomeBar(testContext)
        awesomeBar.setOnStopListener {
            stopped = true
        }

        awesomeBar.listener!!.invoke()

        assertTrue(stopped)
    }

    @Test
    fun `throw exception if provider returns duplicate IDs`() {
        val awesomeBar = BrowserAwesomeBar(testContext)

        val suggestions = listOf(
            AwesomeBar.Suggestion(id = "dupe", score = 0, provider = BrokenProvider()),
            AwesomeBar.Suggestion(id = "dupe", score = 0, provider = BrokenProvider())
        )

        try {
            awesomeBar.processProviderSuggestions(suggestions)
            fail("Expected IllegalStateException for duplicate suggestion IDs")
        } catch (e: IllegalStateException) {
            assertTrue(e.message!!.contains(BrokenProvider::class.java.simpleName))
        }
    }

    @Test
    fun `get unique suggestion id`() {
        val awesomeBar = BrowserAwesomeBar(testContext)

        val suggestion1 = AwesomeBar.Suggestion(id = "http://mozilla.org/1", score = 0, provider = mock())
        assertEquals(1, awesomeBar.getUniqueSuggestionId(suggestion1))

        val suggestion2 = AwesomeBar.Suggestion(id = "http://mozilla.org/2", score = 0, provider = mock())
        assertEquals(2, awesomeBar.getUniqueSuggestionId(suggestion2))

        assertEquals(1, awesomeBar.getUniqueSuggestionId(suggestion1))

        val suggestion3 = AwesomeBar.Suggestion(id = "http://mozilla.org/3", score = 0, provider = mock())
        assertEquals(3, awesomeBar.getUniqueSuggestionId(suggestion3))
    }

    @Test
    fun `unique suggestion id cache has sufficient space`() {
        val awesomeBar = BrowserAwesomeBar(testContext)
        val provider = mockProvider()

        awesomeBar.addProviders(provider)

        for (i in 1..PROVIDER_MAX_SUGGESTIONS) {
            awesomeBar.getUniqueSuggestionId(AwesomeBar.Suggestion(id = "$i", score = 0, provider = provider))
        }

        awesomeBar.getUniqueSuggestionId(AwesomeBar.Suggestion(id = "21", score = 0, provider = provider))

        assertEquals(1, awesomeBar.getUniqueSuggestionId(AwesomeBar.Suggestion(id = "1", score = 0, provider = provider)))
    }

    @Test
    fun `containsProvider - checks if provider exists`() {
        val awesomeBar = BrowserAwesomeBar(testContext)
        val provider1 = mockProvider()
        `when`(provider1.id).thenReturn("1")

        val provider2 = mockProvider()
        `when`(provider2.id).thenReturn("2")

        awesomeBar.addProviders(provider1)
        assertTrue(awesomeBar.containsProvider(provider1))
        assertFalse(awesomeBar.containsProvider(provider2))
    }

    @Test(expected = IllegalStateException::class)
    fun `exception thrown when duplicate provider added`() {
        val awesomeBar = BrowserAwesomeBar(testContext)
        val provider1 = mockProvider()
        `when`(provider1.id).thenReturn("1")

        val provider2 = mockProvider()
        `when`(provider2.id).thenReturn("2")

        val provider3 = mockProvider()
        `when`(provider3.id).thenReturn("1")

        awesomeBar.addProviders(provider1, provider2, provider3)
    }

    @Test(expected = IllegalStateException::class)
    fun `exception thrown when provider added that already exists`() {
        val awesomeBar = BrowserAwesomeBar(testContext)
        val provider1 = mockProvider()
        `when`(provider1.id).thenReturn("1")

        val provider2 = mockProvider()
        `when`(provider2.id).thenReturn("2")

        val provider3 = mockProvider()
        `when`(provider3.id).thenReturn("1")

        awesomeBar.addProviders(provider1)
        awesomeBar.addProviders(provider2, provider3)
    }

    private fun mockProvider(): AwesomeBar.SuggestionProvider = spy(object : AwesomeBar.SuggestionProvider {
        override val id: String = UUID.randomUUID().toString()

        override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
            return emptyList()
        }
    })

    class BrokenProvider : AwesomeBar.SuggestionProvider {
        override val id: String = "Broken"

        override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
            return emptyList()
        }
    }
}

/**
 * Block current thread while root job in [BrowserAwesomeBar] is not completed
 *
 * TODO Remove this workaround when `BrowserAwesomeBar` will allow inject custom `jobDispatcher`
 */
private fun BrowserAwesomeBar.awaitForAllJobsToFinish() {
    job?.let { job ->
        runBlocking { job.join() }
    }
}

/**
 * Block current thread for some amount of time (but not more than [timeoutMs]) for [predicate] to become true.
 *
 * @param timeoutMs max time to wait and then just continue executing
 * @param predicate waiting for this to be true
 *
 * TODO Remove this workaround when `BrowserAwesomeBar` will allow inject custom `jobDispatcher`
 */
private fun awaitFor(timeoutMs: Long = DEFAULT_AWAIT_TIMEOUT, predicate: () -> Boolean) {
    var executionTime = 0L

    while (!predicate() && executionTime < timeoutMs) {
        runBlocking { delay(DEFAULT_AWAIT_DELAY) }
        executionTime += DEFAULT_AWAIT_DELAY
    }
}

private const val DEFAULT_AWAIT_TIMEOUT = 1000L
private const val DEFAULT_AWAIT_DELAY = 10L
