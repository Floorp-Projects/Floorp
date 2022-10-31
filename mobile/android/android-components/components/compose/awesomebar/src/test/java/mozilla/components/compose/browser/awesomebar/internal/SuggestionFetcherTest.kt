/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar.internal

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import mozilla.components.concept.awesomebar.AwesomeBar.SuggestionProvider
import mozilla.components.concept.awesomebar.AwesomeBar.SuggestionProviderGroup
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.spy

@ExperimentalCoroutinesApi // for runTestOnMain
class SuggestionFetcherTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `GIVEN a new fetch request THEN all previous queries are cancelled`() = runTestOnMain {
        val provider: SuggestionProvider = mock()
        val providerGroup = SuggestionProviderGroup(listOf(provider))
        val fetcher = spy(SuggestionFetcher(listOf(providerGroup), null))
        val previousFetchJob: Job = mock()
        fetcher.fetchJob = previousFetchJob
        doAnswer {}.`when`(fetcher).fetchFrom(any(), any(), any(), any())
        val orderVerifier = inOrder(previousFetchJob, fetcher)

        fetcher.fetch("test")

        orderVerifier.verify(previousFetchJob)!!.cancel()
        orderVerifier.verify(fetcher).fetchFrom(providerGroup, provider, "test", null)
    }
}
