/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.AppStore

class AppActionTest {

    private val capture = CaptureActionsMiddleware<AppState, AppAction>()
    private val appStore = AppStore(middlewares = listOf(capture))

    @Test
    fun `WHEN UpdateInactiveExpanded is dispatched THEN update inactiveTabsExpanded`() {
        assertFalse(appStore.state.inactiveTabsExpanded)

        appStore.dispatch(AppAction.UpdateInactiveExpanded(true)).joinBlocking()

        assertTrue(appStore.state.inactiveTabsExpanded)
    }
}
