/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.glean.private.EventMetricType
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.internal.CounterMetric
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.AppMenu
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.HomeMenu
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.Translations
import org.mozilla.fenix.components.menu.middleware.MenuTelemetryMiddleware
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class MenuTelemetryMiddlewareTest {
    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `WHEN adding a bookmark THEN record the bookmark browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.AddBookmark).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "bookmark")
    }

    @Test
    fun `WHEN navigating to edit a bookmark THEN record the bookmark browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.EditBookmark).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "bookmark")
    }

    @Test
    fun `WHEN navigating to bookmarks THEN record the bookmarks browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.Bookmarks).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "bookmarks")
    }

    @Test
    fun `WHEN navigating to customize homepage THEN record the customize homepage telemetry`() {
        val store = createStore()
        assertNull(AppMenu.customizeHomepage.testGetValue())
        assertNull(HomeScreen.customizeHomeClicked.testGetValue())

        store.dispatch(MenuAction.Navigate.CustomizeHomepage).joinBlocking()

        assertTelemetryRecorded(AppMenu.customizeHomepage)
        assertTelemetryRecorded(HomeScreen.customizeHomeClicked)
    }

    @Test
    fun `WHEN navigating to downloads THEN record the downloads browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.Downloads).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "downloads")
    }

    @Test
    fun `WHEN navigating to the help SUMO page THEN record the help interaction telemetry`() {
        val store = createStore()
        assertNull(HomeMenu.helpTapped.testGetValue())

        store.dispatch(MenuAction.Navigate.Help).joinBlocking()

        assertTelemetryRecorded(HomeMenu.helpTapped)
    }

    @Test
    fun `WHEN navigating to history THEN record the history browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.History).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "history")
    }

    @Test
    fun `WHEN navigating to manage extensions THEN record the manage extensions browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.ManageExtensions).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "addons_manager")
    }

    @Test
    fun `WHEN navigating to sync account THEN record the sync account browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(
            MenuAction.Navigate.MozillaAccount(
                accountState = AccountState.NotAuthenticated,
                accesspoint = MenuAccessPoint.Browser,
            ),
        ).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "sync_account")
        assertTelemetryRecorded(AppMenu.signIntoSync)
    }

    @Test
    fun `WHEN opening a new tab THEN record the new tab browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.NewTab).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "new_tab")
    }

    @Test
    fun `WHEN navigating to passwords THEN record the passwords browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.Passwords).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "passwords")
    }

    @Test
    fun `WHEN navigating to the release notes page THEN record the whats new interaction telemetry`() {
        val store = createStore()
        assertNull(HomeMenu.helpTapped.testGetValue())

        store.dispatch(MenuAction.Navigate.ReleaseNotes).joinBlocking()

        assertTelemetryRecorded(Events.whatsNewTapped)
    }

    @Test
    fun `WHEN navigating to the save to collection sheet THEN record the share browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.SaveToCollection(hasCollection = true)).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "save_to_collection")
    }

    @Test
    fun `WHEN navigating to the share sheet THEN record the share browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.Navigate.Share).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "share")
    }

    @Test
    fun `GIVEN the menu accesspoint is from the browser WHEN navigating to the settings THEN record the settings browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())
        assertNull(HomeMenu.settingsItemClicked.testGetValue())

        store.dispatch(MenuAction.Navigate.Settings).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "settings")
        assertNull(HomeMenu.settingsItemClicked.testGetValue())
    }

    @Test
    fun `GIVEN the menu accesspoint is from the home screen WHEN navigating to the settings THEN record the home menu interaction telemetry`() {
        val store = createStore(accessPoint = MenuAccessPoint.Home)
        assertNull(Events.browserMenuAction.testGetValue())
        assertNull(HomeMenu.settingsItemClicked.testGetValue())

        store.dispatch(MenuAction.Navigate.Settings).joinBlocking()

        assertTelemetryRecorded(HomeMenu.settingsItemClicked)
        assertNull(Events.browserMenuAction.testGetValue())
    }

    @Test
    fun `WHEN navigating to translations dialog THEN record the translate browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())
        assertNull(Translations.action.testGetValue())

        store.dispatch(MenuAction.Navigate.Translate).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "translate")

        val snapshot = Translations.action.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("main_flow_browser", snapshot.single().extra?.getValue("item"))
    }

    @Test
    fun `WHEN deleting browsing data and quitting THEN record the quit browser menu telemetry`() {
        val store = createStore()
        assertNull(Events.browserMenuAction.testGetValue())

        store.dispatch(MenuAction.DeleteBrowsingDataAndQuit).joinBlocking()

        assertTelemetryRecorded(Events.browserMenuAction, item = "quit")
    }

    private fun assertTelemetryRecorded(
        event: EventMetricType<Events.BrowserMenuActionExtra>,
        item: String,
    ) {
        assertNotNull(event.testGetValue())

        val snapshot = event.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals(item, snapshot.single().extra?.getValue("item"))
    }

    private fun assertTelemetryRecorded(event: EventMetricType<NoExtras>) {
        assertNotNull(event.testGetValue())
        assertEquals(1, event.testGetValue()!!.size)
    }

    private fun assertTelemetryRecorded(event: CounterMetric) {
        assertNotNull(event.testGetValue())
        assertEquals(1, event.testGetValue()!!)
    }

    private fun createStore(
        menuState: MenuState = MenuState(),
        accessPoint: MenuAccessPoint = MenuAccessPoint.Browser,
    ) = MenuStore(
        initialState = menuState,
        middleware = listOf(
            MenuTelemetryMiddleware(
                accessPoint = accessPoint,
            ),
        ),
    )
}
