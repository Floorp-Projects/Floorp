package org.mozilla.fenix.components.searchengine

import android.content.Context
import android.graphics.Bitmap
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.provider.SearchEngineList
import mozilla.components.browser.search.provider.localization.LocaleSearchLocalizationProvider
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class FenixSearchEngineProviderTest {

    private lateinit var fenixSearchEngineProvider: FenixSearchEngineProvider

    @Before
    fun before() {
        fenixSearchEngineProvider = FakeFenixSearchEngineProvider(testContext)
    }

    /*
    TODO TEST:
        - public API happy path
        - list ordering
        - deduping
        - the above after adding/removing
     */

    @Test
    fun `GIVEN sharedprefs does not contain installed engines WHEN installedSearchEngineIdentifiers THEN defaultEngines + customEngines ids are returned`() = runBlockingTest {
        val expectedDefaults = fenixSearchEngineProvider.baseSearchEngines.toIdSet()
        val expectedCustom = fenixSearchEngineProvider.customSearchEngines.toIdSet()
        val expected = expectedDefaults + expectedCustom

        val actual = fenixSearchEngineProvider.installedSearchEngineIdentifiers(testContext)
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN sharedprefs contains installed engines WHEN installedSearchEngineIdentifiers THEN defaultEngines + customEngines ids are returned`() = runBlockingTest {
        val sp = testContext.getSharedPreferences(FenixSearchEngineProvider.PREF_FILE, Context.MODE_PRIVATE)
        sp.edit().putStringSet(fenixSearchEngineProvider.localeAwareInstalledEnginesKey(), persistedInstalledEngines).apply()

        val expectedStored = persistedInstalledEngines
        val expectedCustom = fenixSearchEngineProvider.customSearchEngines.toIdSet()
        val expected = expectedStored + expectedCustom

        val actual = fenixSearchEngineProvider.installedSearchEngineIdentifiers(testContext)
        assertEquals(expected, actual)
    }
}

private suspend fun Deferred<SearchEngineList>.toIdSet() =
    await().list.map { it.identifier }.toSet()

private val persistedInstalledEngines = setOf("bing", "ecosia")

class FakeFenixSearchEngineProvider(context: Context) : FenixSearchEngineProvider(context) {
    override val localizationProvider: SearchLocalizationProvider
        get() = LocaleSearchLocalizationProvider()

    override var baseSearchEngines: Deferred<SearchEngineList>
        set(_) { throw NotImplementedError("Setting not currently supported on this fake") }
        get() {
            val google = mockSearchEngine(id = "google-b-1-m", n = "Google")

            return CompletableDeferred(
                SearchEngineList(
                    listOf(
                        google,
                        mockSearchEngine("bing", "Bing"),
                        mockSearchEngine("amazondotcom", "Amazon.com")
                    ), default = google
                )
            )
        }

    override val bundledSearchEngines = CompletableDeferred(
        SearchEngineList(
            listOf(
                mockSearchEngine("ecosia", "Ecosia"),
                mockSearchEngine("reddit", "Reddit"),
                mockSearchEngine("startpage", "Startpage.com")
            ), default = null
        )
    )

    override var customSearchEngines: Deferred<SearchEngineList>
        get() {
            return CompletableDeferred(
                SearchEngineList(
                    listOf(
                        mockSearchEngine("my custom site", "my custom site")
                    ), default = null
                )
            )
        }
        set(_) = throw NotImplementedError("Setting not currently supported on this fake")

    override fun updateBaseSearchEngines() { }

    private fun mockSearchEngine(
        id: String,
        n: String = id
    ): SearchEngine {
        // Uses Mockito because of a strange Mockk error. Feel free to rewrite
        return mock(SearchEngine::class.java).apply {
            `when`(identifier).thenReturn(id)
            `when`(name).thenReturn(n)
            `when`(icon).thenReturn(mock(Bitmap::class.java))
        }
    }
}
