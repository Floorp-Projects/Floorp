/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.LocaleAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class LocaleMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Before
    fun setUp() {
        LocaleManager.clear(testContext)
    }

    @Test
    @Ignore("Failing intermittently. To be fixed for https://github.com/mozilla-mobile/android-components/issues/9954")
    @Config(qualifiers = "en-rUS")
    fun `GIVEN a locale has been chosen in the app WHEN we restore state THEN locale is retrieved from storage`() = runTest {
        val localeManager = spy(LocaleManager)
        val currentLocale = localeManager.getCurrentLocale(testContext)
        assertNull(currentLocale)

        val localeMiddleware = spy(
            LocaleMiddleware(
                testContext,
                coroutineContext = dispatcher,
                localeManager = localeManager,
            ),
        )

        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(localeMiddleware),
        )

        assertEquals(store.state.locale, null)

        store.dispatch(LocaleAction.RestoreLocaleStateAction).joinBlocking()
        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        assertEquals(store.state.locale, currentLocale)
    }

    @Test
    @Config(qualifiers = "en-rUS")
    fun `WHEN we update the locale THEN the locale manager is updated`() = runTest {
        val localeManager = spy(LocaleManager)
        val currentLocale = localeManager.getCurrentLocale(testContext)
        assertNull(currentLocale)

        val localeMiddleware = spy(
            LocaleMiddleware(
                testContext,
                coroutineContext = dispatcher,
                localeManager = localeManager,
            ),
        )

        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(localeMiddleware),
        )

        assertEquals(store.state.locale, null)

        val newLocale = "es".toLocale()
        store.dispatch(LocaleAction.UpdateLocaleAction(newLocale)).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()

        verify(localeManager).setNewLocale(testContext, locale = newLocale)
    }
}
