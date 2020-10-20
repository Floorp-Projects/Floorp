package org.mozilla.fenix.components.searchengine

import android.content.Context
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
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
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class FenixSearchEngineProviderTest {

    private lateinit var fenixSearchEngineProvider: FenixSearchEngineProvider

    @Before
    fun before() {
        fenixSearchEngineProvider = FakeFenixSearchEngineProvider(testContext)
        mockkObject(CustomSearchEngineStore)
        fenixSearchEngineProvider.let {
            every { CustomSearchEngineStore.loadCustomSearchEngines(testContext) } returns listOf(
                (it as FakeFenixSearchEngineProvider)
                    .mockSearchEngine("my custom site", "my custom site")
            )
        }
    }

    /*
    TODO TEST:
        - public API happy path
        - list ordering
        - deduping
        - the above after adding/removing
     */

    @Suppress("DEPRECATION")
    @Test
    fun `add custom engine`() = runBlockingTest {
        val engineName = "Ecosia"
        val engineQuery = "www.ecosia.com/%s"
        val searchEngine: SearchEngine = mockk(relaxed = true)
        every { searchEngine.getSearchTemplate() } returns engineQuery
        every { searchEngine.name } returns engineName
        mockkObject(CustomSearchEngineStore)
        coEvery {
            CustomSearchEngineStore.addSearchEngine(
                testContext,
                engineName,
                engineQuery
            )
        } just Runs

        fenixSearchEngineProvider.installSearchEngine(testContext, searchEngine, true)

        coVerify { CustomSearchEngineStore.addSearchEngine(testContext, engineName, engineQuery) }
    }

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
        val sp = testContext.getSharedPreferences(
            FenixSearchEngineProvider.PREF_FILE_SEARCH_ENGINES,
            Context.MODE_PRIVATE
        )
        sp.edit().putStringSet(
            fenixSearchEngineProvider.localeAwareInstalledEnginesKey(),
            persistedInstalledEngines
        ).apply()

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

    override val fallbackEngines: Deferred<SearchEngineList>
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

    override var customSearchEngines: Deferred<SearchEngineList> = CompletableDeferred(
        SearchEngineList(
            listOf(
                mockSearchEngine("my custom site", "my custom site")
            ), default = null
        )
    )

    override fun updateBaseSearchEngines() { }

    fun mockSearchEngine(
        id: String,
        n: String = id
    ): SearchEngine {
        val engine = mockk<SearchEngine>()
        every { engine.identifier } returns id
        every { engine.name } returns n
        every { engine.icon } returns mockk()
        return engine
    }
}
