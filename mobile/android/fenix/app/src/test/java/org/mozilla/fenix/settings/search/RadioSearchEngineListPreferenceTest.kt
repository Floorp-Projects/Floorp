/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SearchState
import mozilla.components.feature.search.SearchUseCases
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class RadioSearchEngineListPreferenceTest {

    private lateinit var radioSearchEngineListPreference: RadioSearchEngineListPreference
    private lateinit var searchUseCases: SearchUseCases
    private lateinit var searchState: SearchState
    private lateinit var testContext: Context
    private lateinit var defaultSearchEngine: SearchEngine
    private lateinit var otherSearchEngine: SearchEngine

    @Before
    fun before() {
        testContext = mockk(relaxed = true)
        searchUseCases = mockk(relaxed = true)

        defaultSearchEngine = mockk {
            every { id } returns "default"
        }
        otherSearchEngine = mockk {
            every { id } returns "other"
        }
        val engineList = listOf(defaultSearchEngine, otherSearchEngine)

        searchState = SearchState(customSearchEngines = engineList)

        every { testContext.components.useCases.searchUseCases } returns searchUseCases
        every { testContext.components.core.store.state.search } returns searchState

        radioSearchEngineListPreference = spyk(RadioSearchEngineListPreference(testContext))
    }

    @Test
    fun `when deleting default search engine a new one is selected`() {
        every { radioSearchEngineListPreference.showUndoSnackbar(any(), any(), any()) } just Runs

        radioSearchEngineListPreference.deleteSearchEngine(
            testContext,
            defaultSearchEngine,
            true
        )

        verify { searchUseCases.removeSearchEngine(defaultSearchEngine) }
        verify { searchUseCases.selectSearchEngine(otherSearchEngine) }
        verify {
            radioSearchEngineListPreference.showUndoSnackbar(
                testContext,
                defaultSearchEngine,
                true
            )
        }
    }

    @Test
    fun `restoreSearchEngine ads engine and makes it default if it was the default before deletion`() {
        radioSearchEngineListPreference.restoreSearchEngine(
            defaultSearchEngine,
            true
        )

        verify { searchUseCases.addSearchEngine(defaultSearchEngine) }
        verify { searchUseCases.selectSearchEngine(defaultSearchEngine) }
    }

    @Test
    fun `restoreSearchEngine ads engine and it doe NOT make it default if it was NOT the default before deletion`() {
        radioSearchEngineListPreference.restoreSearchEngine(
            otherSearchEngine,
            false
        )

        verify { searchUseCases.addSearchEngine(otherSearchEngine) }
        verify(exactly = 0) { searchUseCases.selectSearchEngine(defaultSearchEngine) }
    }
}
