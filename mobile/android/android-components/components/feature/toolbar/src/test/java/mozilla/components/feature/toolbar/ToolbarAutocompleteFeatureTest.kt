/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.domains.Domain
import mozilla.components.browser.domains.autocomplete.BaseDomainAutocompleteProvider
import mozilla.components.browser.domains.autocomplete.DomainList
import mozilla.components.browser.storage.memory.InMemoryHistoryStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ToolbarAutocompleteFeatureTest {

    class TestToolbar : Toolbar {
        override var siteTrackingProtection: Toolbar.SiteTrackingProtection =
            Toolbar.SiteTrackingProtection.OFF_GLOBALLY
        override var title: String = ""
        override var url: CharSequence = ""
        override var siteSecure: Toolbar.SiteSecurity = Toolbar.SiteSecurity.INSECURE
        override var private: Boolean = false

        var autocompleteFilter: (suspend (String, AutocompleteDelegate) -> Unit)? = null

        override fun setSearchTerms(searchTerms: String) {
            fail()
        }

        override fun displayProgress(progress: Int) {
            fail()
        }

        override fun onBackPressed(): Boolean {
            fail()
            return false
        }

        override fun setOnUrlCommitListener(listener: (String) -> Boolean) {
            fail()
        }

        override fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) {
            autocompleteFilter = filter
        }

        override fun addBrowserAction(action: Toolbar.Action) {
            fail()
        }

        override fun addPageAction(action: Toolbar.Action) {
            fail()
        }

        override fun addNavigationAction(action: Toolbar.Action) {
            fail()
        }

        override fun setOnEditListener(listener: Toolbar.OnEditListener) {
            fail()
        }

        override fun displayMode() {
            fail()
        }

        override fun editMode() {
            fail()
        }

        override fun addEditAction(action: Toolbar.Action) {
            fail()
        }
    }

    @Test
    fun `feature can be used without providers`() {
        val toolbar = TestToolbar()

        ToolbarAutocompleteFeature(toolbar)

        assertNotNull(toolbar.autocompleteFilter)

        val autocompleteDelegate: AutocompleteDelegate = mock()
        runBlocking {
            toolbar.autocompleteFilter!!("moz", autocompleteDelegate)
        }
        verify(autocompleteDelegate, never()).applyAutocompleteResult(any())
        verify(autocompleteDelegate, times(1)).noAutocompleteResult("moz")
    }

    @Test
    fun `feature can be configured with providers`() {
        val toolbar = TestToolbar()
        var feature = ToolbarAutocompleteFeature(toolbar)
        val autocompleteDelegate: AutocompleteDelegate = mock()

        var history: HistoryStorage = InMemoryHistoryStorage()
        val domains = object : BaseDomainAutocompleteProvider(DomainList.CUSTOM, { emptyList() }) {
            fun testDomains(list: List<Domain>) {
                domains = list
            }
        }

        // Can autocomplete with just an empty history provider.
        feature.addHistoryStorageProvider(history)
        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")

        // Can autocomplete with a non-empty history provider.
        runBlocking {
            history.recordVisit("https://www.mozilla.org", VisitType.TYPED)
        }

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")
        verifyAutocompleteResult(toolbar, autocompleteDelegate, "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "memoryHistory", totalItems = 1
            )
        )

        // Can autocomplete with just an empty domain provider.
        feature = ToolbarAutocompleteFeature(toolbar)
        feature.addDomainProvider(domains)

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")

        // Can autocomplete with a non-empty domain provider.
        domains.testDomains(listOf(
            Domain.create("https://www.mozilla.org")
        ))

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")
        verifyAutocompleteResult(toolbar, autocompleteDelegate, "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "custom",
                totalItems = 1
            )
        )

        // Can autocomplete with empty history and domain providers.
        history = InMemoryHistoryStorage()
        domains.testDomains(listOf())
        feature.addHistoryStorageProvider(history)

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")

        // Can autocomplete with both domains providing data; test that history is prioritized,
        // falling back to domains.
        domains.testDomains(listOf(
            Domain.create("https://www.mozilla.org"),
            Domain.create("https://moscow.ru")
        ))

        verifyAutocompleteResult(toolbar, autocompleteDelegate, "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "custom",
                totalItems = 2
            )
        )

        runBlocking {
            history.recordVisit("https://www.mozilla.org", VisitType.TYPED)
        }

        verifyAutocompleteResult(toolbar, autocompleteDelegate, "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "memoryHistory",
                totalItems = 1
            )
        )

        verifyAutocompleteResult(toolbar, autocompleteDelegate, "mos",
            AutocompleteResult(
                input = "mos",
                text = "moscow.ru",
                url = "https://moscow.ru",
                source = "custom",
                totalItems = 2
            )
        )
    }

    @Suppress("SameParameterValue")
    private fun verifyNoAutocompleteResult(toolbar: TestToolbar, autocompleteDelegate: AutocompleteDelegate, query: String) {
        runBlocking {
            toolbar.autocompleteFilter!!(query, autocompleteDelegate)
        }
        verify(autocompleteDelegate, never()).applyAutocompleteResult(any())
        verify(autocompleteDelegate, times(1)).noAutocompleteResult(query)
        reset(autocompleteDelegate)
    }

    private fun verifyAutocompleteResult(toolbar: TestToolbar, autocompleteDelegate: AutocompleteDelegate, query: String, result: AutocompleteResult) {
        runBlocking {
            toolbar.autocompleteFilter!!.invoke(query, autocompleteDelegate)
        }
        verify(autocompleteDelegate, times(1)).applyAutocompleteResult(result)
        verify(autocompleteDelegate, never()).noAutocompleteResult(query)
        reset(autocompleteDelegate)
    }
}
