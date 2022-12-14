/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.graphics.Bitmap
import android.graphics.Bitmap.Config.ARGB_8888
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.core.graphics.applyCanvas
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.search.SearchEngine.Type.BUNDLED
import mozilla.components.concept.menu.Orientation
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.UnifiedSearch
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.search.SearchDialogFragmentStore
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.SearchFragmentAction.SearchDefaultEngineSelected
import org.mozilla.fenix.search.SearchFragmentAction.SearchHistoryEngineSelected
import org.mozilla.fenix.search.SearchFragmentState
import org.mozilla.fenix.utils.Settings
import java.util.UUID

@RunWith(FenixRobolectricTestRunner::class)
class SearchSelectorToolbarActionTest {

    private lateinit var store: SearchDialogFragmentStore

    @MockK(relaxed = true)
    private lateinit var menu: SearchSelectorMenu

    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        store = SearchDialogFragmentStore(testSearchFragmentState)

        every { testContext.settings() } returns settings
    }

    @Test
    fun `WHEN search selector toolbar action is clicked THEN the search selector menu is shown`() {
        val action = spyk(
            SearchSelectorToolbarAction(
                store = store,
                defaultSearchEngine = null,
                menu = menu,
            ),
        )
        val view = action.createView(LinearLayout(testContext) as ViewGroup) as SearchSelector
        assertNull(UnifiedSearch.searchMenuTapped.testGetValue())

        every { settings.shouldUseBottomToolbar } returns false
        view.performClick()

        assertNotNull(UnifiedSearch.searchMenuTapped.testGetValue())
        verify {
            menu.menuController.show(view, Orientation.DOWN)
        }

        every { settings.shouldUseBottomToolbar } returns true
        view.performClick()

        assertNotNull(UnifiedSearch.searchMenuTapped.testGetValue())
        verify {
            menu.menuController.show(view, Orientation.UP)
        }
    }

    @Test
    fun `GIVEN a binded search selector View WHEN a search engine is selected THEN update the icon`() {
        mockkStatic("org.mozilla.fenix.search.toolbar.SearchSelectorToolbarActionKt") {
            val searchEngineIcon: BitmapDrawable = mockk(relaxed = true)
            every { any<SearchEngine>().getScaledIcon(any()) } returns searchEngineIcon
            val selector = SearchSelectorToolbarAction(store, mockk(), mockk())
            val view = spyk(SearchSelector(testContext))

            selector.bind(view)
            store.dispatch(
                SearchDefaultEngineSelected(
                    engine = testSearchEngine,
                    settings = mockk(relaxed = true),
                ),
            )
            store.waitUntilIdle()

            verify { testSearchEngine.getScaledIcon(any()) }
            verify {
                view.setIcon(
                    icon = searchEngineIcon,
                    contentDescription = testSearchEngine.name,
                )
            }
        }
    }

    @Test
    fun `GIVEN the same view is binded multiple times WHEN the search engine changes THEN update the icon only once`() {
        // This scenario with the same View binded multiple times can happen after a "invalidateActions" call.
        mockkStatic("org.mozilla.fenix.search.toolbar.SearchSelectorToolbarActionKt") {
            val searchEngineIcon: BitmapDrawable = mockk(relaxed = true)
            every { any<SearchEngine>().getScaledIcon(any()) } returns searchEngineIcon
            val selector = SearchSelectorToolbarAction(store, mockk(), mockk())
            val view = spyk(SearchSelector(testContext))

            selector.bind(view)
            selector.bind(view)
            selector.bind(view)
            store.dispatch(
                SearchDefaultEngineSelected(
                    engine = testSearchEngine,
                    settings = mockk(relaxed = true),
                ),
            )
            store.waitUntilIdle()

            verify { testSearchEngine.getScaledIcon(any()) }
            verify(exactly = 1) {
                view.setIcon(
                    icon = searchEngineIcon,
                    contentDescription = testSearchEngine.name,
                )
            }
        }
    }

    @Test
    fun `GIVEN a binded search selector View WHEN a search engine is selected THEN update the icon only if a different search engine is selected`() {
        mockkStatic("org.mozilla.fenix.search.toolbar.SearchSelectorToolbarActionKt") {
            val searchEngineIcon: BitmapDrawable = mockk(relaxed = true)
            every { any<SearchEngine>().getScaledIcon(any()) } returns searchEngineIcon
            val selector = SearchSelectorToolbarAction(store, mockk(), mockk())
            val view = spyk(SearchSelector(testContext))

            // Test an initial change
            selector.bind(view)
            store.dispatch(
                SearchDefaultEngineSelected(
                    engine = testSearchEngine,
                    settings = mockk(relaxed = true),
                ),
            )
            store.waitUntilIdle()
            verify(exactly = 1) { testSearchEngine.getScaledIcon(any()) }
            verify(exactly = 1) {
                view.setIcon(
                    icon = searchEngineIcon,
                    contentDescription = testSearchEngine.name,
                )
            }

            // Test the same search engine being selected
            store.dispatch(
                SearchDefaultEngineSelected(
                    engine = testSearchEngine,
                    settings = mockk(relaxed = true),
                ),
            )
            store.waitUntilIdle()
            verify(exactly = 1) { testSearchEngine.getScaledIcon(any()) }
            verify(exactly = 1) {
                view.setIcon(
                    icon = searchEngineIcon,
                    contentDescription = testSearchEngine.name,
                )
            }

            // Test another search engine being selected
            val newSearchEngine = testSearchEngine.copy(
                name = "NewSearchEngine",
            )
            store.dispatch(
                SearchHistoryEngineSelected(
                    engine = newSearchEngine,
                ),
            )
            store.waitUntilIdle()
            verify(exactly = 1) { testSearchEngine.getScaledIcon(any()) }
            verify(exactly = 1) { newSearchEngine.getScaledIcon(any()) }
            verify(exactly = 1) {
                view.setIcon(
                    icon = searchEngineIcon,
                    contentDescription = testSearchEngine.name,
                )
            }
            verify(exactly = 1) {
                view.setIcon(
                    icon = searchEngineIcon,
                    contentDescription = newSearchEngine.name,
                )
            }
        }
    }

    @Test
    fun `GIVEN a search engine WHEN asking for a scaled icon THEN return a drawable with a fixed size`() {
        val originalIcon = Bitmap.createBitmap(100, 100, ARGB_8888).applyCanvas {
            drawColor(Color.RED)
        }
        val expectedScaledIcon = Bitmap.createBitmap(
            testContext.resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size),
            testContext.resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size),
            ARGB_8888,
        ).applyCanvas {
            drawColor(Color.RED)
        }
        val searchEngine = testSearchEngine.copy(
            icon = originalIcon,
        )

        val result = searchEngine.getScaledIcon(testContext)

        // Check dimensions, config and pixel data
        assertTrue(expectedScaledIcon.sameAs(result.bitmap))
    }
}

private val testSearchFragmentState = SearchFragmentState(
    query = "https://example.com",
    url = "https://example.com",
    searchTerms = "search terms",
    searchEngineSource = SearchEngineSource.None,
    defaultEngine = null,
    showSearchTermHistory = true,
    showSearchSuggestions = false,
    showSearchShortcutsSetting = false,
    showSearchSuggestionsHint = false,
    showSearchShortcuts = false,
    areShortcutsAvailable = false,
    showClipboardSuggestions = false,
    showHistorySuggestionsForCurrentEngine = true,
    showAllHistorySuggestions = false,
    showBookmarkSuggestions = false,
    showSyncedTabsSuggestions = false,
    showSessionSuggestions = true,
    tabId = "tabId",
    pastedText = "",
    searchAccessPoint = MetricsUtils.Source.SHORTCUT,
)

private val testSearchEngine = SearchEngine(
    id = UUID.randomUUID().toString(),
    name = "testSearchEngine",
    icon = mockk(),
    type = BUNDLED,
    resultUrls = listOf(
        "https://www.startpage.com/sp/search?q={searchTerms}",
    ),
)
