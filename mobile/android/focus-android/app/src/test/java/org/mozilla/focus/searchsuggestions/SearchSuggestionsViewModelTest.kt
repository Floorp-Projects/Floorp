/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.Observer
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TestRule
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SearchSuggestionsViewModelTest {
    @get:Rule
    var rule: TestRule = InstantTaskExecutorRule()

    @Mock
    private lateinit var observer: Observer<String?>

    private lateinit var lifecycle: LifecycleRegistry
    private lateinit var viewModel: SearchSuggestionsViewModel

    @Before
    fun setup() {
        lifecycle = LifecycleRegistry(mock(LifecycleOwner::class.java))
        MockitoAnnotations.openMocks(this)

        viewModel = SearchSuggestionsViewModel(ApplicationProvider.getApplicationContext())
    }

    @Test
    fun setSearchQuery() {
        viewModel.searchQuery.observeForever(observer)

        viewModel.setSearchQuery("Mozilla")
        verify(observer).onChanged("Mozilla")
    }

    @Test
    fun alwaysSearchSelected() {
        viewModel.selectedSearchSuggestion.observeForever(observer)

        viewModel.selectSearchSuggestion("mozilla.com", "google", true)
        verify(observer).onChanged("mozilla.com")
        assertEquals(true, viewModel.alwaysSearch)
    }
}
