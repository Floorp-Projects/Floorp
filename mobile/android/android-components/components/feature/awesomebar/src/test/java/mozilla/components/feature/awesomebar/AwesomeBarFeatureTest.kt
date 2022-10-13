/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar

import android.content.res.Resources
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.DEFAULT_HISTORY_SUGGESTION_LIMIT
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class AwesomeBarFeatureTest {

    @Test
    fun `Feature connects toolbar with awesome bar`() {
        val toolbar: Toolbar = mock()
        val awesomeBar: AwesomeBar = mock()
        doReturn(View(testContext)).`when`(awesomeBar).asView()

        var listener: Toolbar.OnEditListener? = null

        `when`(toolbar.setOnEditListener(any())).thenAnswer { invocation ->
            listener = invocation.getArgument<Toolbar.OnEditListener>(0)
            Unit
        }

        AwesomeBarFeature(awesomeBar, toolbar)

        assertNotNull(listener)

        listener!!.onStartEditing()

        verify(awesomeBar).onInputStarted()

        listener!!.onTextChanged("Hello")

        verify(awesomeBar).onInputChanged("Hello")

        listener!!.onStopEditing()

        verify(awesomeBar).onInputCancelled()
    }

    @Test
    fun `Feature connects awesome bar with toolbar`() {
        val toolbar: Toolbar = mock()
        val awesomeBar: AwesomeBar = mock()

        var stopListener: (() -> Unit)? = null

        `when`(awesomeBar.setOnStopListener(any())).thenAnswer { invocation ->
            stopListener = invocation.getArgument<() -> Unit>(0)
            Unit
        }

        AwesomeBarFeature(awesomeBar, toolbar)

        assertNotNull(stopListener)

        stopListener!!.invoke()

        verify(toolbar).displayMode()
    }

    @Test
    fun `addSessionProvider adds provider`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())
        val resources: Resources = mock()
        `when`(resources.getString(Mockito.anyInt())).thenReturn("Switch to tab")

        verify(awesomeBar, never()).addProviders(any())

        feature.addSessionProvider(resources, mock(), mock())

        verify(awesomeBar).addProviders(any())
    }

    @Test
    fun `addSearchProvider adds provider with specified search engine`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())

        verify(awesomeBar, never()).addProviders(any())

        val searchEngine: SearchEngine = mock()
        feature.addSearchProvider(searchEngine, mock(), mock())

        val provider = argumentCaptor<SearchSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(searchEngine, provider.value.client.searchEngine)
    }

    @Test
    fun `addSearchProvider adds provider for default search engine`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())

        verify(awesomeBar, never()).addProviders(any())

        val store: BrowserStore = mock()
        feature.addSearchProvider(testContext, store = store, searchUseCase = mock(), fetchClient = mock())

        val provider = argumentCaptor<SearchSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(store, provider.value.client.store)
        assertNull(provider.value.client.searchEngine)
    }

    @Test
    fun `addSearchProvider adds browser engine to suggestion provider`() {
        val engine: Engine = mock()
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())
        feature.addSearchProvider(testContext, mock(), mock(), mock(), engine = engine)

        val provider = argumentCaptor<SearchSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(engine, provider.value.engine)

        feature.addSearchProvider(mock(), mock(), mock(), engine = engine)
        verify(awesomeBar, times(2)).addProviders(provider.capture())
        assertSame(engine, provider.allValues.last().engine)
    }

    @Test
    fun `addHistoryProvider adds provider`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())

        verify(awesomeBar, never()).addProviders(any())

        feature.addHistoryProvider(mock(), mock())

        verify(awesomeBar).addProviders(any())
    }

    @Test
    fun `addHistoryProvider adds browser engine to suggestion provider`() {
        val engine: Engine = mock()
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())
        feature.addHistoryProvider(mock(), mock(), engine = engine)

        val provider = argumentCaptor<HistoryStorageSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(engine, provider.value.engine)
    }

    @Test
    fun `addHistoryProvider adds the limit of suggestions to be returned to suggestion provider if positive`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())
        feature.addHistoryProvider(
            historyStorage = mock(),
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 42,
        )

        val provider = argumentCaptor<HistoryStorageSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(42, provider.value.maxNumberOfSuggestions)
    }

    @Test
    fun `addHistoryProvider does not add the limit of suggestions to be returned to suggestion provider if negative`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())
        feature.addHistoryProvider(
            historyStorage = mock(),
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = -1,
        )

        val provider = argumentCaptor<HistoryStorageSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(DEFAULT_HISTORY_SUGGESTION_LIMIT, provider.value.maxNumberOfSuggestions)
    }

    @Test
    fun `addHistoryProvider does not add the limit of suggestions to be returned to suggestion provider if 0`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())
        feature.addHistoryProvider(
            historyStorage = mock(),
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 0,
        )

        val provider = argumentCaptor<HistoryStorageSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(DEFAULT_HISTORY_SUGGESTION_LIMIT, provider.value.maxNumberOfSuggestions)
    }

    @Test
    fun `addClipboardProvider adds provider`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())

        verify(awesomeBar, never()).addProviders(any())

        feature.addClipboardProvider(testContext, mock())

        verify(awesomeBar).addProviders(any())
    }

    @Test
    fun `addClipboardProvider adds browser engine to suggestion provider`() {
        val engine: Engine = mock()
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())
        feature.addClipboardProvider(testContext, mock(), engine = engine)

        val provider = argumentCaptor<ClipboardSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(engine, provider.value.engine)
    }

    @Test
    fun `addSearchActionProvider adds provider`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())

        verify(awesomeBar, never()).addProviders(any())

        feature.addSearchActionProvider(mock(), mock())

        verify(awesomeBar).addProviders(any())
    }

    @Test
    fun `Feature invokes custom start and complete hooks`() {
        val toolbar: Toolbar = mock()
        val awesomeBar: AwesomeBar = mock()

        var startInvoked = false
        var completeInvoked = false

        var listener: Toolbar.OnEditListener? = null

        `when`(toolbar.setOnEditListener(any())).thenAnswer { invocation ->
            listener = invocation.getArgument<Toolbar.OnEditListener>(0)
            Unit
        }

        AwesomeBarFeature(
            awesomeBar,
            toolbar,
            onEditStart = { startInvoked = true },
            onEditComplete = { completeInvoked = true },
        )

        assertFalse(startInvoked)
        assertFalse(completeInvoked)

        listener!!.onStartEditing()

        assertTrue(startInvoked)
        assertFalse(completeInvoked)
        startInvoked = false

        listener!!.onStopEditing()

        assertFalse(startInvoked)
        assertTrue(completeInvoked)
    }
}
