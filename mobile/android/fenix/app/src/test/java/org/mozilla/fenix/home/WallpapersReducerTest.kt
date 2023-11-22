package org.mozilla.fenix.home

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

class WallpapersReducerTest {
    @Test
    fun `WHEN OpenToHome dispatched THEN mode is updated to normal`() {
        val appStore = AppStore(AppState(mode = BrowsingMode.Private))

        appStore.dispatch(AppAction.WallpaperAction.OpenToHome).joinBlocking()

        assertEquals(BrowsingMode.Normal, appStore.state.mode)
    }
}
