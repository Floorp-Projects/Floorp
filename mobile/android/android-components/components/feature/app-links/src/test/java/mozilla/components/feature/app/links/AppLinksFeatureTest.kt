/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import androidx.fragment.app.FragmentManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.AppIntentState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class AppLinksFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var store: BrowserStore
    private lateinit var mockContext: Context
    private lateinit var mockFragmentManager: FragmentManager
    private lateinit var mockUseCases: AppLinksUseCases
    private lateinit var mockGetRedirect: AppLinksUseCases.GetAppLinkRedirect
    private lateinit var mockOpenRedirect: AppLinksUseCases.OpenAppLinkRedirect
    private lateinit var mockEngineSession: EngineSession
    private lateinit var mockDialog: RedirectDialogFragment
    private lateinit var mockLoadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase
    private lateinit var feature: AppLinksFeature

    private val webUrl = "https://example.com"
    private val webUrlWithAppLink = "https://soundcloud.com"
    private val intentUrl = "zxing://scan"
    private val aboutUrl = "about://scan"

    @Before
    fun setup() {
        store = BrowserStore()
        mockContext = mock()

        mockFragmentManager = mock()
        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock())
        mockUseCases = mock()
        mockEngineSession = mock()
        mockDialog = mock()
        mockLoadUrlUseCase = mock()

        mockGetRedirect = mock()
        mockOpenRedirect = mock()
        `when`(mockUseCases.interceptedAppLinkRedirect).thenReturn(mockGetRedirect)
        `when`(mockUseCases.openAppLink).thenReturn(mockOpenRedirect)

        val webRedirect = AppLinkRedirect(null, webUrl, null)
        val appRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), null, null)
        val appRedirectFromWebUrl = AppLinkRedirect(Intent.parseUri(webUrlWithAppLink, 0), null, null)

        `when`(mockGetRedirect.invoke(webUrl)).thenReturn(webRedirect)
        `when`(mockGetRedirect.invoke(intentUrl)).thenReturn(appRedirect)
        `when`(mockGetRedirect.invoke(webUrlWithAppLink)).thenReturn(appRedirectFromWebUrl)

        feature = spy(
            AppLinksFeature(
                context = mockContext,
                store = store,
                fragmentManager = mockFragmentManager,
                useCases = mockUseCases,
                dialog = mockDialog,
                loadUrlUseCase = mockLoadUrlUseCase,
            ),
        ).also {
            it.start()
        }
    }

    @After
    fun teardown() {
        feature.stop()
    }

    @Test
    fun `feature observes app intents when started`() {
        val tab = createTab(webUrl)
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify(feature, never()).handleAppIntent(any(), any(), any())

        val intent: Intent = mock()
        val appIntent = AppIntentState(intentUrl, intent)
        store.dispatch(ContentAction.UpdateAppIntentAction(tab.id, appIntent)).joinBlocking()

        val tabWithPendingAppIntent = store.state.findTab(tab.id)!!
        assertNotNull(tabWithPendingAppIntent.content.appIntent)

        verify(feature).handleAppIntent(tabWithPendingAppIntent, intentUrl, intent)

        store.waitUntilIdle()
        val tabWithConsumedAppIntent = store.state.findTab(tab.id)!!
        assertNull(tabWithConsumedAppIntent.content.appIntent)
    }

    @Test
    fun `feature doesn't observes app intents when stopped`() {
        val tab = createTab(webUrl)
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        verify(feature, never()).handleAppIntent(any(), any(), any())

        feature.stop()

        val intent: Intent = mock()
        val appIntent = AppIntentState(intentUrl, intent)
        store.dispatch(ContentAction.UpdateAppIntentAction(tab.id, appIntent)).joinBlocking()

        verify(feature, never()).handleAppIntent(any(), any(), any())
    }

    @Test
    fun `in non-private mode an external app is opened without a dialog`() {
        val tab = createTab(webUrl)
        feature.handleAppIntent(tab, intentUrl, mock())

        verifyNoMoreInteractions(mockDialog)
        verify(mockOpenRedirect).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `in private mode an external app dialog is shown`() {
        val tab = createTab(webUrl, private = true)
        feature.handleAppIntent(tab, intentUrl, mock())

        verify(mockDialog).showNow(eq(mockFragmentManager), anyString())
        verify(mockOpenRedirect, never()).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `reused redirect dialog if exists`() {
        val tab = createTab(webUrl, private = true)
        feature.handleAppIntent(tab, intentUrl, mock())

        val dialog = feature.getOrCreateDialog()
        assertEquals(dialog, feature.getOrCreateDialog())
    }

    @Test
    fun `redirect dialog is only added once`() {
        val tab = createTab(webUrl, private = true)
        feature.handleAppIntent(tab, intentUrl, mock())

        verify(mockDialog).showNow(eq(mockFragmentManager), anyString())

        doReturn(mockDialog).`when`(feature).getOrCreateDialog()
        doReturn(mockDialog).`when`(mockFragmentManager).findFragmentByTag(RedirectDialogFragment.FRAGMENT_TAG)
        feature.handleAppIntent(tab, intentUrl, mock())
        verify(mockDialog, times(1)).showNow(mockFragmentManager, RedirectDialogFragment.FRAGMENT_TAG)
    }

    @Test
    fun `only loads URL if scheme is supported`() {
        val tab = createTab(webUrl, private = true)

        feature.loadUrlIfSchemeSupported(tab, intentUrl)
        verify(mockLoadUrlUseCase, never()).invoke(anyString(), anyString(), any(), any())

        feature.loadUrlIfSchemeSupported(tab, webUrl)
        verify(mockLoadUrlUseCase, times(1)).invoke(anyString(), anyString(), any(), any())

        feature.loadUrlIfSchemeSupported(tab, aboutUrl)
        verify(mockLoadUrlUseCase, times(2)).invoke(anyString(), anyString(), any(), any())
    }
}
