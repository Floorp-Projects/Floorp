/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.domains.Domain
import mozilla.components.browser.domains.autocomplete.BaseDomainAutocompleteProvider
import mozilla.components.browser.domains.autocomplete.DomainList
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.AutocompleteProvider
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class ToolbarAutocompleteFeatureTest {
    class TestToolbar : Toolbar {
        override var highlight: Toolbar.Highlight = Toolbar.Highlight.NONE
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

        override fun onStop() {
            fail()
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

        override fun removeBrowserAction(action: Toolbar.Action) {
            fail()
        }

        override fun removePageAction(action: Toolbar.Action) {
            fail()
        }

        override fun addPageAction(action: Toolbar.Action) {
            fail()
        }

        override fun addNavigationAction(action: Toolbar.Action) {
            fail()
        }

        override fun removeNavigationAction(action: Toolbar.Action) {
            fail()
        }

        override fun setOnEditListener(listener: Toolbar.OnEditListener) {
            fail()
        }

        override fun displayMode() {
            fail()
        }

        override fun editMode(cursorPlacement: Toolbar.CursorPlacement) {
            fail()
        }

        override fun addEditActionStart(action: Toolbar.Action) {
            fail()
        }

        override fun addEditActionEnd(action: Toolbar.Action) {
            fail()
        }

        override fun removeEditActionEnd(action: Toolbar.Action) {
            fail()
        }

        override fun hideMenuButton() {
            fail()
        }

        override fun showMenuButton() {
            fail()
        }

        override fun invalidateActions() {
            fail()
        }

        override fun dismissMenu() {
            fail()
        }

        override fun enableScrolling() {
            fail()
        }

        override fun disableScrolling() {
            fail()
        }

        override fun collapse() {
            fail()
        }

        override fun expand() {
            fail()
        }
    }

    @Test
    fun `feature can be used without providers`() = runTest {
        val toolbar = TestToolbar()

        ToolbarAutocompleteFeature(toolbar)

        assertNotNull(toolbar.autocompleteFilter)

        val autocompleteDelegate: AutocompleteDelegate = mock()
        toolbar.autocompleteFilter!!("moz", autocompleteDelegate)

        verify(autocompleteDelegate, never()).applyAutocompleteResult(any(), any())
        verify(autocompleteDelegate, times(1)).noAutocompleteResult("moz")
    }

    @Test
    fun `feature can be configured with providers`() = runTest {
        val toolbar = TestToolbar()
        var feature = ToolbarAutocompleteFeature(toolbar)
        val autocompleteDelegate: AutocompleteDelegate = mock()

        var history: AutocompleteProvider = mock()
        val domains = object : BaseDomainAutocompleteProvider(DomainList.CUSTOM, { emptyList() }, 22) {
            fun testDomains(list: List<Domain>) {
                domains = list
            }
        }

        // Can autocomplete with just an empty history provider.
        feature.addAutocompleteProvider(history)
        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")

        // Can autocomplete with a non-empty history provider.
        doReturn(
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "memoryHistory",
                totalItems = 1,
            ),
        ).`when`(history).getAutocompleteSuggestion("mo")

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")
        verifyAutocompleteResult(
            toolbar,
            autocompleteDelegate,
            "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "memoryHistory",
                totalItems = 1,
            ),
        )

        // Can autocomplete with just an empty domain provider.
        feature = ToolbarAutocompleteFeature(toolbar)
        feature.addAutocompleteProvider(domains)

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")

        // Can autocomplete with a non-empty domain provider.
        domains.testDomains(
            listOf(
                Domain.create("https://www.mozilla.org"),
            ),
        )

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")
        verifyAutocompleteResult(
            toolbar,
            autocompleteDelegate,
            "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "custom",
                totalItems = 1,
            ),
        )

        // Can autocomplete with empty history and domain providers.
        history = spy(AutocompleteProviderFake()) // use a real object so that the comparator will work
        domains.testDomains(listOf())
        feature.addAutocompleteProvider(history)

        verifyNoAutocompleteResult(toolbar, autocompleteDelegate, "hi")

        // Can autocomplete with both domains providing data; test that history is prioritized,
        // falling back to domains.
        domains.testDomains(
            listOf(
                Domain.create("https://www.mozilla.org"),
                Domain.create("https://moscow.ru"),
            ),
        )

        verifyAutocompleteResult(
            toolbar,
            autocompleteDelegate,
            "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "custom",
                totalItems = 2,
            ),
        )

        doReturn(
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "memoryHistory",
                totalItems = 1,
            ),
        ).`when`(history).getAutocompleteSuggestion("mo")

        verifyAutocompleteResult(
            toolbar,
            autocompleteDelegate,
            "mo",
            AutocompleteResult(
                input = "mo",
                text = "mozilla.org",
                url = "https://www.mozilla.org",
                source = "memoryHistory",
                totalItems = 1,
            ),
        )

        verifyAutocompleteResult(
            toolbar,
            autocompleteDelegate,
            "mos",
            AutocompleteResult(
                input = "mos",
                text = "moscow.ru",
                url = "https://moscow.ru",
                source = "custom",
                totalItems = 2,
            ),
        )
    }

    @Test
    fun `feature triggers speculative connect for results if engine provided`() = runTest {
        val toolbar = TestToolbar()
        val engine: Engine = mock()
        val feature = ToolbarAutocompleteFeature(toolbar, engine) { true }
        val autocompleteDelegate: AutocompleteDelegate = mock()

        val domains = object : BaseDomainAutocompleteProvider(DomainList.CUSTOM, { emptyList() }) {
            fun testDomains(list: List<Domain>) {
                domains = list
            }
        }
        domains.testDomains(listOf(Domain.create("https://www.mozilla.org")))
        feature.addAutocompleteProvider(domains)

        toolbar.autocompleteFilter!!.invoke("mo", autocompleteDelegate)

        val callbackCaptor = argumentCaptor<() -> Unit>()
        verify(autocompleteDelegate, times(1)).applyAutocompleteResult(any(), callbackCaptor.capture())
        verify(engine, never()).speculativeConnect("https://www.mozilla.org")
        callbackCaptor.value.invoke()
        verify(engine).speculativeConnect("https://www.mozilla.org")
    }

    @Test
    fun `WHEN should autocomplete returns false THEN return no results`() = runTest {
        val toolbar = TestToolbar()
        val engine: Engine = mock()
        var shouldAutoComplete = false
        val feature = ToolbarAutocompleteFeature(toolbar, engine) { shouldAutoComplete }
        val autocompleteDelegate: AutocompleteDelegate = mock()

        val domains = object : BaseDomainAutocompleteProvider(DomainList.CUSTOM, { emptyList() }) {
            fun testDomains(list: List<Domain>) {
                domains = list
            }
        }
        domains.testDomains(listOf(Domain.create("https://www.mozilla.org")))
        feature.addAutocompleteProvider(domains)

        toolbar.autocompleteFilter!!.invoke("mo", autocompleteDelegate)

        verify(autocompleteDelegate, times(1)).noAutocompleteResult(any())
        verify(engine, never()).speculativeConnect("https://www.mozilla.org")

        shouldAutoComplete = true
        toolbar.autocompleteFilter!!.invoke("mo", autocompleteDelegate)

        val callbackCaptor = argumentCaptor<() -> Unit>()
        verify(autocompleteDelegate, times(1)).applyAutocompleteResult(any(), callbackCaptor.capture())
        verify(engine, never()).speculativeConnect("https://www.mozilla.org")
        callbackCaptor.value.invoke()
        verify(engine).speculativeConnect("https://www.mozilla.org")
    }

    @Test
    fun `GIVEN no autocomplete providers WHEN checking for autocomplete results THEN silently fail with no results`() = runTest {
        val toolbar = TestToolbar()
        val engine: Engine = mock()
        val feature = ToolbarAutocompleteFeature(toolbar, engine) { true }
        val autocompleteDelegate: AutocompleteDelegate = mock()

        val domains = object : BaseDomainAutocompleteProvider(DomainList.CUSTOM, { emptyList() }) {
            fun testDomains(list: List<Domain>) {
                domains = list
            }
        }
        domains.testDomains(listOf(Domain.create("https://www.mozilla.org")))

        feature.addAutocompleteProvider(domains)
        toolbar.autocompleteFilter!!.invoke("mo", autocompleteDelegate)
        val callbackCaptor = argumentCaptor<() -> Unit>()
        verify(autocompleteDelegate, times(1)).applyAutocompleteResult(any(), callbackCaptor.capture())
        verify(engine, never()).speculativeConnect("https://www.mozilla.org")
        callbackCaptor.value.invoke()
        verify(engine).speculativeConnect("https://www.mozilla.org")

        // After checking the results for when a provider exists test what happens when no providers exist.
        feature.removeAutocompleteProvider(domains)
        toolbar.autocompleteFilter!!.invoke("mo", autocompleteDelegate)
        verify(autocompleteDelegate, times(1)).noAutocompleteResult(any())
        verify(engine, times(1)).speculativeConnect("https://www.mozilla.org")
    }

    @Test
    fun `GIVEN no initial autocomplete providers and one is added WHEN checking for autocomplete results THEN return autocomplete suggestions`() = runTest {
        val toolbar = TestToolbar()
        val engine: Engine = mock()
        val feature = ToolbarAutocompleteFeature(toolbar, engine) { true }
        val autocompleteDelegate: AutocompleteDelegate = mock()

        val domains = object : BaseDomainAutocompleteProvider(DomainList.CUSTOM, { emptyList() }) {
            fun testDomains(list: List<Domain>) {
                domains = list
            }
        }
        domains.testDomains(listOf(Domain.create("https://www.mozilla.org")))

        toolbar.autocompleteFilter!!.invoke("mo", autocompleteDelegate)
        verify(autocompleteDelegate, times(1)).noAutocompleteResult(any())
        verify(engine, never()).speculativeConnect("https://www.mozilla.org")

        // After checking no results for when no providers exist test what happens when a new one is added.
        feature.addAutocompleteProvider(domains)
        toolbar.autocompleteFilter!!.invoke("mo", autocompleteDelegate)
        val callbackCaptor = argumentCaptor<() -> Unit>()
        verify(autocompleteDelegate, times(1)).applyAutocompleteResult(any(), callbackCaptor.capture())
        verify(engine, never()).speculativeConnect("https://www.mozilla.org")
        callbackCaptor.value.invoke()
        verify(engine).speculativeConnect("https://www.mozilla.org")
    }

    @Test
    fun `GIVEN providers exist WHEN a new one is added THEN they are sorted by their priority`() {
        val feature = ToolbarAutocompleteFeature(mock())

        val provider1 = AutocompleteProviderFake(autocompletePriority = 11).also {
            assertTrue(feature.addAutocompleteProvider(it))
        }
        val provider2 = AutocompleteProviderFake(autocompletePriority = 22).also {
            assertTrue(feature.addAutocompleteProvider(it))
        }
        val provider3 = AutocompleteProviderFake(autocompletePriority = 3).also {
            assertTrue(feature.addAutocompleteProvider(it))
        }

        assertEquals(
            listOf(provider3, provider1, provider2),
            feature.autocompleteProviders.toList(),
        )
    }

    @Test
    fun `GIVEN providers exist WHEN trying to add an existing one THEN avoid adding`() {
        val feature = ToolbarAutocompleteFeature(mock())
        val provider1 = AutocompleteProviderFake(autocompletePriority = 11).also {
            feature.addAutocompleteProvider(it)
        }
        val provider2 = AutocompleteProviderFake(autocompletePriority = 22).also {
            feature.addAutocompleteProvider(it)
        }
        val provider3 = AutocompleteProviderFake(autocompletePriority = 3).also {
            feature.addAutocompleteProvider(it)
        }
        val provider4 = AutocompleteProviderFake(autocompletePriority = 15).also {
            feature.addAutocompleteProvider(it)
        }

        assertTrue(feature.removeAutocompleteProvider(provider4))

        assertEquals(
            listOf(provider3, provider1, provider2),
            feature.autocompleteProviders.toList(),
        )
    }

    @Test
    fun `GIVEN providers don't exist WHEN trying to remove one THEN avoid fail gracefully`() {
        val feature = ToolbarAutocompleteFeature(mock())
        val provider1 = AutocompleteProviderFake(autocompletePriority = 11).also {
            feature.addAutocompleteProvider(it)
        }
        val provider2 = AutocompleteProviderFake(autocompletePriority = 22).also {
            feature.addAutocompleteProvider(it)
        }
        val provider3 = AutocompleteProviderFake(autocompletePriority = 3)

        assertFalse(feature.removeAutocompleteProvider(provider3))

        assertEquals(
            listOf(provider1, provider2),
            feature.autocompleteProviders.toList(),
        )
    }

    @Test
    fun `GIVEN providers exist WHEN removing one THEN keep the other sorted`() {
        val feature = ToolbarAutocompleteFeature(mock())
        val provider1 = AutocompleteProviderFake(autocompletePriority = 11).also {
            assertTrue(feature.addAutocompleteProvider(it))
        }
        val provider2 = AutocompleteProviderFake(autocompletePriority = 22).also {
            assertTrue(feature.addAutocompleteProvider(it))
        }

        assertFalse(feature.addAutocompleteProvider(provider1))

        assertEquals(
            listOf(provider1, provider2),
            feature.autocompleteProviders.toList(),
        )
    }

    @Test
    fun `GIVEN providers exist WHEN when they are updated THEN the old ones are replaced by new ones sorted by priority`() {
        val feature = ToolbarAutocompleteFeature(mock())
        feature.autocompleteProviders = sortedSetOf(
            AutocompleteProviderFake(autocompletePriority = 11),
            AutocompleteProviderFake(autocompletePriority = 22),
        )
        val provider1 = AutocompleteProviderFake(autocompletePriority = 11).also {
            feature.addAutocompleteProvider(it)
        }
        val provider2 = AutocompleteProviderFake(autocompletePriority = 2).also {
            feature.addAutocompleteProvider(it)
        }
        val provider3 = AutocompleteProviderFake().also {
            feature.addAutocompleteProvider(it)
        }

        feature.updateAutocompleteProviders(listOf(provider1, provider2, provider3))

        assertEquals(
            listOf(provider3, provider2, provider1),
            feature.autocompleteProviders.toList(),
        )
    }

    @Test
    fun `GIVEN a request to refresh autocomplete WHEN the providers are updated THEN also refresh autocomplete`() {
        val toolbar: Toolbar = mock()
        val feature = ToolbarAutocompleteFeature(toolbar)

        feature.updateAutocompleteProviders(emptyList(), false)
        verify(toolbar, never()).refreshAutocomplete()

        feature.updateAutocompleteProviders(emptyList(), true)
        verify(toolbar).refreshAutocomplete()
    }

    @Test
    fun `GIVEN a request to refresh autocomplete WHEN a provider is added THEN also refresh autocomplete`() {
        val toolbar: Toolbar = mock()
        val feature = ToolbarAutocompleteFeature(toolbar)

        feature.addAutocompleteProvider(mock(), false)
        verify(toolbar, never()).refreshAutocomplete()

        feature.addAutocompleteProvider(mock(), true)
        verify(toolbar).refreshAutocomplete()
    }

    @Test
    fun `GIVEN a request to refresh autocomplete WHEN a provider is removed THEN also refresh autocomplete`() {
        val toolbar: Toolbar = mock()
        val feature = ToolbarAutocompleteFeature(toolbar)

        feature.removeAutocompleteProvider(mock(), false)
        verify(toolbar, never()).refreshAutocomplete()

        feature.removeAutocompleteProvider(mock(), true)
        verify(toolbar).refreshAutocomplete()
    }

    @Suppress("SameParameterValue")
    private fun verifyNoAutocompleteResult(toolbar: TestToolbar, autocompleteDelegate: AutocompleteDelegate, query: String) = runTest {
        toolbar.autocompleteFilter!!(query, autocompleteDelegate)

        verify(autocompleteDelegate, never()).applyAutocompleteResult(any(), any())
        verify(autocompleteDelegate, times(1)).noAutocompleteResult(query)
        reset(autocompleteDelegate)
    }

    private fun verifyAutocompleteResult(toolbar: TestToolbar, autocompleteDelegate: AutocompleteDelegate, query: String, result: AutocompleteResult) = runTest {
        toolbar.autocompleteFilter!!.invoke(query, autocompleteDelegate)

        verify(autocompleteDelegate, times(1)).applyAutocompleteResult(eq(result), any())
        verify(autocompleteDelegate, never()).noAutocompleteResult(query)
        reset(autocompleteDelegate)
    }
}

/**
 * Empty implementation of [AutocompleteProvider].
 * [getAutocompleteSuggestion] will return `null` by default.
 *
 * @param resultToReturn Optional nullable [AutocompleteResult] to return for all queries.
 */
private class AutocompleteProviderFake(
    val resultToReturn: AutocompleteResult? = null,
    override val autocompletePriority: Int = 0,
) : AutocompleteProvider {
    override suspend fun getAutocompleteSuggestion(query: String) = resultToReturn
}
