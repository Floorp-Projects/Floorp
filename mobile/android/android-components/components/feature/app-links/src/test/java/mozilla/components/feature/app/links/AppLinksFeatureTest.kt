/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import androidx.fragment.app.FragmentManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.AppIntentState
import mozilla.components.browser.state.state.ExternalPackage
import mozilla.components.browser.state.state.PackageCategory
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
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

        store.waitUntilIdle()
        verify(feature).handleAppIntent(any(), any(), any())

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
    fun `WHEN should prompt AND in non-private mode THEN an external app dialog is shown`() {
        feature = spy(
            AppLinksFeature(
                context = mockContext,
                store = store,
                fragmentManager = mockFragmentManager,
                useCases = mockUseCases,
                dialog = mockDialog,
                loadUrlUseCase = mockLoadUrlUseCase,
                shouldPrompt = { true },
            ),
        ).also {
            it.start()
        }

        val tab = createTab(webUrl)
        feature.handleAppIntent(tab, intentUrl, mock())

        verify(mockDialog).showNow(eq(mockFragmentManager), anyString())
        verify(mockOpenRedirect, never()).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `WHEN should not prompt AND in non-private mode THEN an external app dialog is not shown`() {
        feature = spy(
            AppLinksFeature(
                context = mockContext,
                store = store,
                fragmentManager = mockFragmentManager,
                useCases = mockUseCases,
                dialog = mockDialog,
                loadUrlUseCase = mockLoadUrlUseCase,
                shouldPrompt = { false },
            ),
        ).also {
            it.start()
        }

        val tab = createTab(webUrl)
        feature.handleAppIntent(tab, intentUrl, mock())

        verify(mockDialog, never()).showNow(eq(mockFragmentManager), anyString())
    }

    @Test
    fun `WHEN custom tab and caller is the same as external app THEN an external app dialog is not shown`() {
        feature = spy(
            AppLinksFeature(
                context = mockContext,
                store = store,
                fragmentManager = mockFragmentManager,
                useCases = mockUseCases,
                dialog = mockDialog,
                loadUrlUseCase = mockLoadUrlUseCase,
                shouldPrompt = { true },
            ),
        ).also {
            it.start()
        }

        val tab =
            createCustomTab(
                id = "c",
                url = webUrl,
                source = SessionState.Source.External.CustomTab(
                    ExternalPackage("com.zxing.app", PackageCategory.PRODUCTIVITY),
                ),
            )

        val appIntent: Intent = mock()
        val componentName: ComponentName = mock()
        doReturn(componentName).`when`(appIntent).component
        doReturn("com.zxing.app").`when`(componentName).packageName

        feature.handleAppIntent(tab, intentUrl, appIntent)

        verify(mockDialog, never()).showNow(eq(mockFragmentManager), anyString())
    }

    @Test
    fun `WHEN should prompt and in private mode THEN an external app dialog is shown`() {
        feature = spy(
            AppLinksFeature(
                context = mockContext,
                store = store,
                fragmentManager = mockFragmentManager,
                useCases = mockUseCases,
                dialog = mockDialog,
                loadUrlUseCase = mockLoadUrlUseCase,
                shouldPrompt = { true },
            ),
        ).also {
            it.start()
        }

        val tab = createTab(webUrl, private = true)
        feature.handleAppIntent(tab, intentUrl, mock())

        verify(mockDialog).showNow(eq(mockFragmentManager), anyString())
        verify(mockOpenRedirect, never()).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `WHEN should not prompt and in private mode THEN an external app dialog is shown`() {
        feature = spy(
            AppLinksFeature(
                context = mockContext,
                store = store,
                fragmentManager = mockFragmentManager,
                useCases = mockUseCases,
                dialog = mockDialog,
                loadUrlUseCase = mockLoadUrlUseCase,
                shouldPrompt = { false },
            ),
        ).also {
            it.start()
        }

        val tab = createTab(webUrl, private = true)
        feature.handleAppIntent(tab, intentUrl, mock())

        verify(mockDialog).showNow(eq(mockFragmentManager), anyString())
        verify(mockOpenRedirect, never()).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `redirect dialog is only added once`() {
        val tab = createTab(webUrl, private = true)
        feature.handleAppIntent(tab, intentUrl, mock())

        verify(mockDialog).showNow(eq(mockFragmentManager), anyString())

        doReturn(mockDialog).`when`(feature).getOrCreateDialog(false, "")
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

    @Test
    fun `WHEN caller and intent have the same package name THEN return true`() {
        val customTab =
            createCustomTab(
                id = "c",
                url = webUrl,
                source = SessionState.Source.External.CustomTab(
                    ExternalPackage("com.zxing.app", PackageCategory.PRODUCTIVITY),
                ),
            )
        val appIntent: Intent = mock()
        val componentName: ComponentName = mock()
        doReturn(componentName).`when`(appIntent).component
        doReturn("com.zxing.app").`when`(componentName).packageName
        assertTrue(feature.isSameCallerAndApp(customTab, appIntent))

        val tab = createTab(webUrl, private = true)
        assertFalse(feature.isSameCallerAndApp(tab, appIntent))

        val customTab2 =
            createCustomTab(
                id = "c",
                url = webUrl,
                source = SessionState.Source.External.CustomTab(
                    ExternalPackage("com.example.app", PackageCategory.PRODUCTIVITY),
                ),
            )
        assertFalse(feature.isSameCallerAndApp(customTab2, appIntent))

        doReturn(null).`when`(componentName).packageName
        assertFalse(feature.isSameCallerAndApp(customTab, appIntent))
    }
}
