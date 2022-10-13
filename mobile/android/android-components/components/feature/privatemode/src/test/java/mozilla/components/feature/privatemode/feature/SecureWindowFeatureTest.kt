/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.privatemode.feature

import android.view.Window
import android.view.WindowManager.LayoutParams.FLAG_SECURE
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class SecureWindowFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var window: Window
    private val tabId = "test-tab"

    @Before
    fun setup() {
        window = mock()
    }

    @Test
    fun `no-op if no sessions`() {
        val store = BrowserStore(BrowserState(tabs = emptyList()))
        val feature = SecureWindowFeature(window, store)

        feature.start()

        verify(window, never()).addFlags(FLAG_SECURE)
        verify(window, never()).clearFlags(FLAG_SECURE)
    }

    @Test
    fun `add flags to private session`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = tabId, private = true),
                ),
                selectedTabId = tabId,
            ),
        )
        val feature = SecureWindowFeature(window, store)

        feature.start()

        verify(window).addFlags(FLAG_SECURE)
    }

    @Test
    fun `remove flags from normal session`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = tabId, private = false),
                ),
                selectedTabId = tabId,
            ),
        )
        val feature = SecureWindowFeature(window, store)

        feature.start()

        verify(window).clearFlags(FLAG_SECURE)
    }

    @Test
    fun `remove flags on stop`() {
        val store = BrowserStore()
        val feature = SecureWindowFeature(window, store, clearFlagOnStop = true)

        feature.start()
        feature.stop()

        verify(window).clearFlags(FLAG_SECURE)
    }
}
