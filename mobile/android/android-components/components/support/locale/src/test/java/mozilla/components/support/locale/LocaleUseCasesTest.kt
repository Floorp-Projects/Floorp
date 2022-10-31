/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.LocaleAction
import mozilla.components.browser.state.action.LocaleAction.UpdateLocaleAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class LocaleUseCasesTest {

    private lateinit var browserStore: BrowserStore

    @Before
    fun setup() {
        browserStore = mock()
    }

    @Test
    fun `WHEN the locale is updated THEN the browser state reflects the change`() {
        val useCases = LocaleUseCases(browserStore)
        val locale = Locale("MyFavoriteLanguage")

        useCases.notifyLocaleChanged(locale)

        verify(browserStore).dispatch(UpdateLocaleAction(locale))
    }

    @Test
    fun `WHEN state is restored THEN the browser state locale is restored`() {
        val useCases = LocaleUseCases(browserStore)
        useCases.restore()
        verify(browserStore).dispatch(LocaleAction.RestoreLocaleStateAction)
    }
}
