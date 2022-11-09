/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.graphics.drawable.toBitmap
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.edit.EditToolbar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.SearchFragmentState
import java.util.UUID

@RunWith(FenixRobolectricTestRunner::class)
class ToolbarViewTest {
    @MockK(relaxed = true)
    private lateinit var interactor: ToolbarInteractor

    @MockK private lateinit var engine: Engine
    private lateinit var context: Context
    private lateinit var toolbar: BrowserToolbar
    private val defaultState: SearchFragmentState = SearchFragmentState(
        tabId = null,
        url = "",
        searchTerms = "",
        query = "",
        searchEngineSource = SearchEngineSource.Default(
            mockk {
                every { name } returns "Search Engine"
                every { icon } returns testContext.getDrawable(R.drawable.ic_search)!!.toBitmap()
                every { type } returns SearchEngine.Type.BUNDLED
                every { isGeneral } returns true
            },
        ),
        defaultEngine = null,
        showSearchShortcutsSetting = false,
        showSearchSuggestionsHint = false,
        showSearchSuggestions = false,
        showSearchShortcuts = false,
        areShortcutsAvailable = true,
        showClipboardSuggestions = false,
        showHistorySuggestionsForCurrentEngine = true,
        showAllHistorySuggestions = false,
        showBookmarkSuggestions = false,
        showSyncedTabsSuggestions = false,
        showSessionSuggestions = false,
        searchAccessPoint = MetricsUtils.Source.NONE,
    )

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        every { context.settings() } returns mockk(relaxed = true)
        toolbar = spyk(BrowserToolbar(context))
    }

    @Test
    fun `sets toolbar to normal mode`() {
        buildToolbarView(isPrivate = false)
        assertFalse(toolbar.private)
    }

    @Test
    fun `sets toolbar to private mode`() {
        buildToolbarView(isPrivate = true)
        assertTrue(toolbar.private)
    }

    @Test
    fun `View gets initialized only once`() {
        val view = buildToolbarView(false)
        assertFalse(view.isInitialized)

        view.update(defaultState)
        view.update(defaultState)
        view.update(defaultState)

        verify(exactly = 1) { toolbar.setSearchTerms(any()) }

        assertTrue(view.isInitialized)
    }

    @Test
    fun `search term updates the url`() {
        val view = buildToolbarView(false)

        view.update(defaultState)
        view.update(defaultState)
        view.update(defaultState)

        // editMode gets called when the view is initialized.
        verify(exactly = 2) { toolbar.editMode() }
        // search term changes update the url and invoke the interactor.
        verify(exactly = 2) { toolbar.url = any() }
        verify(exactly = 2) { interactor.onTextChanged(any()) }
    }

    @Test
    fun `GIVEN search term is set WHEN switching to edit mode THEN the cursor is set at the end of the search term`() {
        every { context.settings().showUnifiedSearchFeature } returns true
        val view = buildToolbarView(false)
        mockkObject(FeatureFlags)

        view.update(defaultState.copy(searchTerms = "search terms"))

        // editMode gets called when the view is initialized.
        verify(exactly = 1) { toolbar.editMode(Toolbar.CursorPlacement.ALL) }
        verify(exactly = 1) { toolbar.editMode(Toolbar.CursorPlacement.END) }
    }

    @Test
    fun `GIVEN no search term is set WHEN switching to edit mode THEN the cursor is set at the end of the search term`() {
        every { context.settings().showUnifiedSearchFeature } returns true
        val view = buildToolbarView(false)
        mockkObject(FeatureFlags)

        view.update(defaultState)

        // editMode gets called when the view is initialized.
        verify(exactly = 2) { toolbar.editMode(Toolbar.CursorPlacement.ALL) }
    }

    @Test
    fun `URL gets set to the states query`() {
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState.copy(query = "Query"))

        assertEquals("Query", toolbarView.view.url)
    }

    @Test
    fun `URL gets set to the states pastedText if exists`() {
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState.copy(query = "Query", pastedText = "Pasted"))

        assertEquals("Pasted", toolbarView.view.url)
    }

    @Test
    fun `searchTerms get set if pastedText is null or empty`() {
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState.copy(query = "Query", pastedText = "", searchTerms = "Search Terms"))

        verify { toolbar.setSearchTerms("Search Terms") }
    }

    @Test
    fun `searchTerms don't get set if pastedText has a value`() {
        val toolbarView = buildToolbarView(false)
        toolbarView.update(
            defaultState.copy(query = "Query", pastedText = "PastedText", searchTerms = "Search Terms"),
        )

        verify(exactly = 0) { toolbar.setSearchTerms("Search Terms") }
    }

    @Test
    fun `searchEngine name and icon get set on update`() {
        val editToolbar: EditToolbar = mockk(relaxed = true)
        every { toolbar.edit } returns editToolbar

        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState)

        verify { editToolbar.setIcon(any(), "Search Engine") }
    }

    @Test
    fun `GIVEN a general search engine is default WHEN a topic specific engine is selected THEN the hint changes`() {
        val topicSpecificEngine = buildSearchEngine(SearchEngine.Type.BUNDLED, false)
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState)
        assertEquals(context.getString(R.string.search_hint), toolbarView.view.edit.hint)

        toolbarView.update(defaultState.copy(searchEngineSource = SearchEngineSource.Default(topicSpecificEngine)))

        assertEquals(context.getString(R.string.application_search_hint), toolbarView.view.edit.hint)
    }

    @Test
    fun `GIVEN a topic specific engine is default WHEN a general engine is selected THEN the hint changes`() {
        val topicSpecificEngine = buildSearchEngine(SearchEngine.Type.BUNDLED, false)
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState.copy(searchEngineSource = SearchEngineSource.Default(topicSpecificEngine)))
        assertEquals(context.getString(R.string.application_search_hint), toolbarView.view.edit.hint)

        toolbarView.update(defaultState)

        assertEquals(context.getString(R.string.search_hint), toolbarView.view.edit.hint)
    }

    @Test
    fun `GIVEN a topic specific engine is default WHEN a custom engine is selected THEN the hint changes`() {
        val topicSpecificEngine = buildSearchEngine(SearchEngine.Type.BUNDLED, false)
        val customEngine = buildSearchEngine(SearchEngine.Type.CUSTOM, true)
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState.copy(searchEngineSource = SearchEngineSource.Default(topicSpecificEngine)))
        assertEquals(context.getString(R.string.application_search_hint), toolbarView.view.edit.hint)

        toolbarView.update(defaultState.copy(searchEngineSource = SearchEngineSource.Default(customEngine)))

        assertEquals(context.getString(R.string.search_hint), toolbarView.view.edit.hint)
    }

    @Test
    fun `GIVEN a general engine is default WHEN a custom engine is selected THEN the hint does not change`() {
        val customEngine = buildSearchEngine(SearchEngine.Type.CUSTOM, true)
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState)
        assertEquals(context.getString(R.string.search_hint), toolbarView.view.edit.hint)

        toolbarView.update(defaultState.copy(searchEngineSource = SearchEngineSource.Default(customEngine)))

        assertEquals(context.getString(R.string.search_hint), toolbarView.view.edit.hint)
    }

    @Test
    fun `GIVEN a custom engine is default WHEN a general engine is selected THEN the hint does not change`() {
        val customEngine = buildSearchEngine(SearchEngine.Type.CUSTOM, true)
        val toolbarView = buildToolbarView(false)
        toolbarView.update(defaultState.copy(searchEngineSource = SearchEngineSource.Default(customEngine)))
        assertEquals(context.getString(R.string.search_hint), toolbarView.view.edit.hint)

        toolbarView.update(defaultState)

        assertEquals(context.getString(R.string.search_hint), toolbarView.view.edit.hint)
    }

    private fun buildToolbarView(isPrivate: Boolean) = ToolbarView(
        context,
        context.settings(),
        interactor,
        isPrivate = isPrivate,
        view = toolbar,
        fromHomeFragment = false,
    )

    private fun buildSearchEngine(type: SearchEngine.Type, isGeneral: Boolean) = SearchEngine(
        id = UUID.randomUUID().toString(),
        name = "General",
        icon = testContext.getDrawable(R.drawable.ic_search)!!.toBitmap(),
        type = type,
        isGeneral = isGeneral,
    )
}
