/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class BrowserThumbnailsTest {

    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var store: BrowserStore
    private lateinit var engineView: EngineView
    private lateinit var thumbnails: BrowserThumbnails
    private val tabId = "test-tab"

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)
        store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab("https://www.mozilla.org", id = tabId)
                    ),
                    selectedTabId = tabId
                )
            )
        )
        engineView = mock()
        thumbnails = BrowserThumbnails(testContext, engineView, store)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `do not capture thumbnail when feature is stopped and a site finishes loading`() {
        thumbnails.start()
        thumbnails.stop()

        store.dispatch(ContentAction.UpdateThumbnailAction(tabId, mock())).joinBlocking()

        verifyNoMoreInteractions(engineView)
    }

    @Suppress("UNCHECKED_CAST")
    @Test
    fun `feature must capture thumbnail when a site finishes loading and first paint`() {
        val bitmap: Bitmap? = mock()

        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, true)).joinBlocking()

        thumbnails.start()

        `when`(engineView.captureThumbnail(any()))
            .thenAnswer { // if engineView responds with a bitmap
                (it.arguments[0] as (Bitmap?) -> Unit).invoke(bitmap)
            }

        verify(store, never()).dispatch(ContentAction.UpdateThumbnailAction(tabId, bitmap!!))

        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, false)).joinBlocking()
        store.dispatch(ContentAction.UpdateFirstContentfulPaintStateAction(tabId, true)).joinBlocking()

        verify(store).dispatch(ContentAction.UpdateThumbnailAction(tabId, bitmap))
    }

    @Suppress("UNCHECKED_CAST")
    @Test
    fun `feature never updates the store if there is no thumbnail bitmap`() {
        val store: BrowserStore = mock()
        val state: BrowserState = mock()
        val engineView: EngineView = mock()
        val feature = BrowserThumbnails(testContext, engineView, store)

        `when`(store.state).thenReturn(state)
        `when`(engineView.captureThumbnail(any()))
            .thenAnswer { // if engineView responds with a bitmap
                (it.arguments[0] as (Bitmap?) -> Unit).invoke(null)
            }

        feature.requestScreenshot()

        verifyNoInteractions(store)
    }

    @Suppress("UNCHECKED_CAST")
    @Test
    fun `feature never updates the store if there is no tab ID`() {
        val store: BrowserStore = mock()
        val state: BrowserState = mock()
        val engineView: EngineView = mock()
        val feature = BrowserThumbnails(testContext, engineView, store)
        val bitmap: Bitmap = mock()

        `when`(store.state).thenReturn(state)
        `when`(state.selectedTabId).thenReturn(tabId)
        `when`(engineView.captureThumbnail(any()))
            .thenAnswer { // if engineView responds with a bitmap
                (it.arguments[0] as (Bitmap?) -> Unit).invoke(bitmap)
            }

        feature.requestScreenshot()

        verify(store).dispatch(ContentAction.UpdateThumbnailAction(tabId, bitmap))
    }

    @Test
    fun `when a page is loaded and the os is in low memory condition thumbnail should not be captured`() {
        store.dispatch(ContentAction.UpdateThumbnailAction(tabId, mock())).joinBlocking()

        thumbnails.testLowMemory = true

        thumbnails.start()

        verify(engineView, never()).captureThumbnail(any())
    }
}
