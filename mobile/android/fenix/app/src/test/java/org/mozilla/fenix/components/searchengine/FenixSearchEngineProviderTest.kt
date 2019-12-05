package org.mozilla.fenix.components.searchengine

import android.content.Context
import android.graphics.Bitmap
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.provider.SearchEngineList
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import java.util.UUID

@ExperimentalCoroutinesApi
class FenixSearchEngineProviderTest {

    private val testContext = mockk<Context>()

    private lateinit var fenixSearchEngineProvider: FenixSearchEngineProvider

    @Before
    fun before() {
        fenixSearchEngineProvider = FakeFenixSearchEngineProvider(testContext)

        every {
            testContext.getSharedPreferences(FenixSearchEngineProvider.PREF_FILE, Context.MODE_PRIVATE)
        } returns mockk(relaxed = true)
    }

    /*
    TODO TEST:
        - public API happy path
        - list ordering
        - deduping
        - the above after adding/removing
     */

    @Test
    fun `temp test class inits`() = runBlockingTest {
        val t = fenixSearchEngineProvider.loadSearchEngines(testContext)

        println(t)
    }



}

class FakeFenixSearchEngineProvider(context: Context) : FenixSearchEngineProvider(context) {
    override val defaultEngines: Deferred<SearchEngineList>
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

    override val bundledEngines = CompletableDeferred(
        SearchEngineList(
            listOf(
                mockSearchEngine("ecosia", "Ecosia"),
                mockSearchEngine("reddit", "Reddit"),
                mockSearchEngine("startpage", "Startpage.com")
            ), default = null
        )
    )

    override var customEngines: Deferred<SearchEngineList>
        get() {
            return CompletableDeferred(
                SearchEngineList(
                    listOf(
                        mockSearchEngine("my custom site", "my custom site")
                    ), default = null
                )
            )
        }
        set(_) = throw RuntimeException("Setting not currently supported on this fake")

    private fun mockSearchEngine(
        id: String,
        n: String = id
        // TODO this fails with `Missing calls inside every { ... } block`. Not sure why
//    ) = mockk<SearchEngine> {
//        every { identifier } returns id
//        every { name } returns n
//        every { icon } returns mockk()
//    }
    ): SearchEngine {
        return mock(SearchEngine::class.java).apply {
            `when`(identifier).thenReturn(id)
            `when`(name).thenReturn(n)
            `when`(icon).thenReturn(mock(Bitmap::class.java))
        }
    }
}
