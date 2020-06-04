package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.app.links.AppLinkRedirect
import mozilla.components.feature.app.links.AppLinksUseCases
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

class DefaultToolbarMenuTest {

    private lateinit var defaultToolbarMenu: DefaultToolbarMenu

    private val context: Context = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {}
    private val lifecycleOwner: LifecycleOwner = mockk(relaxed = true)
    private val bookmarksStorage: BookmarksStorage = mockk(relaxed = true)

    private val store: BrowserStore = BrowserStore(initialState = BrowserState(
        listOf(
            createTab("https://www.mozilla.org", id = "readerable-tab", readerState = ReaderState(readerable = true))
        )
    ))

    @Before
    fun setUp() {
        defaultToolbarMenu = spyk(
            DefaultToolbarMenu(
                context,
                sessionManager,
                store,
                hasAccountProblem = false,
                shouldReverseItems = false,
                onItemTapped = onItemTapped,
                lifecycleOwner = lifecycleOwner,
                bookmarksStorage = bookmarksStorage
            )
        )

        val settings = Settings.getInstance(context, true)
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        every { context.settings() } returns settings
    }

    @Test
    fun `get all low prio highlight items`() {
        every { context.components.useCases.webAppUseCases.isPinningSupported() } returns true
        every { context.components.useCases.webAppUseCases.isInstallable() } returns true

        val getAppLinkRedirect: AppLinksUseCases.GetAppLinkRedirect = mockk(relaxed = true)
        every { context.components.useCases.appLinksUseCases.appLinkRedirect } returns getAppLinkRedirect
        val appLinkRedirect: AppLinkRedirect = mockk(relaxed = true)
        every { appLinkRedirect.hasExternalApp() } returns true
        every { getAppLinkRedirect(any()) } returns appLinkRedirect

        val list = defaultToolbarMenu.getLowPrioHighlightItems()

        assertEquals(ToolbarMenu.Item.InstallToHomeScreen, list[0])
        assertEquals(ToolbarMenu.Item.OpenInApp, list[1])
    }

    @Test
    fun `get all low prio highlight items without AddToHomeScreen`() {
        val settings = Settings.getInstance(context, true)
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        every { context.settings() } returns settings

        every { context.components.useCases.webAppUseCases.isPinningSupported() } returns false

        val getAppLinkRedirect: AppLinksUseCases.GetAppLinkRedirect = mockk(relaxed = true)
        every { context.components.useCases.appLinksUseCases.appLinkRedirect } returns getAppLinkRedirect
        val appLinkRedirect: AppLinkRedirect = mockk(relaxed = true)
        every { appLinkRedirect.hasExternalApp() } returns true
        every { getAppLinkRedirect(any()) } returns appLinkRedirect

        val list = defaultToolbarMenu.getLowPrioHighlightItems()

        assertEquals(ToolbarMenu.Item.OpenInApp, list[0])
    }

    @Test
    fun `get all low prio highlight items without OpenInApp`() {
        every { context.components.useCases.webAppUseCases.isPinningSupported() } returns true
        every { context.components.useCases.webAppUseCases.isInstallable() } returns true

        val getAppLinkRedirect: AppLinksUseCases.GetAppLinkRedirect = mockk(relaxed = true)
        every { context.components.useCases.appLinksUseCases.appLinkRedirect } returns getAppLinkRedirect
        every { getAppLinkRedirect(any()).hasExternalApp() } returns false

        val list = defaultToolbarMenu.getLowPrioHighlightItems()

        assertEquals(ToolbarMenu.Item.InstallToHomeScreen, list[0])
    }
}
