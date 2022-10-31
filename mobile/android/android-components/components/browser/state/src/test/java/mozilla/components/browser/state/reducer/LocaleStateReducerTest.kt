/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.LocaleAction
import mozilla.components.browser.state.state.BrowserState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.util.Locale

class LocaleStateReducerTest {
    @Test
    fun `WHEN updating locale action is called THEN the locale state is updated`() {
        val reducer = LocaleStateReducer
        val state = BrowserState()
        val locale = Locale("es")
        val action = LocaleAction.UpdateLocaleAction(locale)

        assertNull(state.locale)

        val result = reducer.reduce(state, action)

        assertEquals(result.locale, locale)
    }

    @Test
    fun `WHEN restoring locale action is called THEN the locale state is persisted`() {
        val reducer = LocaleStateReducer
        val state = BrowserState()
        val action = LocaleAction.RestoreLocaleStateAction

        assertNull(state.locale)

        val result = reducer.reduce(state, action)

        assertNull(result.locale)
    }
}
