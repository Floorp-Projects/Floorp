package org.mozilla.focus.searchsuggestions

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleOwner
import android.arch.lifecycle.LifecycleRegistry
import org.junit.Test

import org.junit.Assert.assertEquals
import org.junit.Rule
import org.mockito.Mockito.mock
import android.arch.core.executor.testing.InstantTaskExecutorRule
import org.junit.rules.TestRule

class SearchSuggestionsViewModelTest {
    @get:Rule
    var rule: TestRule = InstantTaskExecutorRule()

    @Test
    fun setSearchQuery() {
        val service = mock(SearchSuggestionsService::class.java)
        val preferences = mock(SearchSuggestionsPreferences::class.java)
        val lifecycle = LifecycleRegistry(mock(LifecycleOwner::class.java))
        lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)

        val viewModel = SearchSuggestionsViewModel(service, preferences)

        viewModel.setSearchQuery("Mozilla")

        assertEquals("Mozilla", viewModel.searchQuery.value)
    }
}
