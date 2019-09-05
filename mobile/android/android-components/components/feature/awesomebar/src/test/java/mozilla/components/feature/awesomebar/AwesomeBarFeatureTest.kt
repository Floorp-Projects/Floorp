/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

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

        verify(awesomeBar, never()).addProviders(any())

        feature.addSessionProvider(mock(), mock())

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

        val context = testContext
        val searchEngineManager: SearchEngineManager = mock()
        feature.addSearchProvider(context, searchEngineManager, mock(), mock())

        val provider = argumentCaptor<SearchSuggestionProvider>()
        verify(awesomeBar).addProviders(provider.capture())
        assertSame(searchEngineManager, provider.value.client.searchEngineManager)
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
    fun `addClipboardProvider adds provider`() {
        val awesomeBar: AwesomeBar = mock()

        val feature = AwesomeBarFeature(awesomeBar, mock())

        verify(awesomeBar, never()).addProviders(any())

        feature.addClipboardProvider(testContext, mock())

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
            onEditComplete = { completeInvoked = true })

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
