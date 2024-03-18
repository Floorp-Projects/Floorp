/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test
import java.util.Locale

class LocaleActionTest {
    @Test
    fun `WHEN a new locale is selected THEN it is updated in the store`() {
        val store = BrowserStore(BrowserState())
        val locale1 = Locale("es")
        store.dispatch(LocaleAction.UpdateLocaleAction(locale1)).joinBlocking()
        assertEquals(locale1, store.state.locale)
    }

    @Test
    fun `WHEN the state is restored from disk THEN the store receives the state`() {
        val store = BrowserStore(BrowserState())

        val state = store.state
        store.dispatch(LocaleAction.RestoreLocaleStateAction).joinBlocking()
        assertSame(state, store.state)
    }
}
