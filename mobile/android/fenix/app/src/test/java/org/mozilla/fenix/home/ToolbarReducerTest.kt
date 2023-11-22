package org.mozilla.fenix.home

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

class ToolbarReducerTest {
    @Test
    fun `WHEN new tab clicked THEN mode is updated to normal`() {
        val appStore = AppStore(AppState(mode = BrowsingMode.Private))

        appStore.dispatch(AppAction.ToolbarAction.NewTab).joinBlocking()

        assertEquals(BrowsingMode.Normal, appStore.state.mode)
    }

    @Test
    fun `WHEN new private tab clicked THEN mode is updated to private`() {
        val appStore = AppStore(AppState(mode = BrowsingMode.Normal))

        appStore.dispatch(AppAction.ToolbarAction.NewPrivateTab).joinBlocking()

        assertEquals(BrowsingMode.Private, appStore.state.mode)
    }
}
