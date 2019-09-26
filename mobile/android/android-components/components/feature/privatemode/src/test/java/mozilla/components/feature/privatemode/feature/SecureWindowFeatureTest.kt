/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.privatemode.feature

import android.view.Window
import android.view.WindowManager.LayoutParams.FLAG_SECURE
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class SecureWindowFeatureTest {

    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var window: Window
    private val tabId = "test-tab"

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)
        window = mock()
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
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
                    createTab("https://www.mozilla.org", id = tabId, private = true)
                ),
                selectedTabId = tabId
            )
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
                    createTab("https://www.mozilla.org", id = tabId, private = false)
                ),
                selectedTabId = tabId
            )
        )
        val feature = SecureWindowFeature(window, store)

        feature.start()

        verify(window).clearFlags(FLAG_SECURE)
    }
}
