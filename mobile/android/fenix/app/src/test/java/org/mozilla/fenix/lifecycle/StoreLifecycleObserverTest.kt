/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lifecycle

import androidx.test.core.app.ApplicationProvider
import io.mockk.mockk
import io.mockk.verifySequence
import mozilla.components.browser.state.action.AppLifecycleAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class StoreLifecycleObserverTest {
    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `WHEN onPause is called THEN dispatch PauseAction`() {
        val appStore: AppStore = mockk(relaxed = true)
        val browserStore: BrowserStore = mockk(relaxed = true)
        val observer = StoreLifecycleObserver(appStore, browserStore)

        observer.onPause(mockk())

        verifySequence {
            appStore.dispatch(AppAction.AppLifecycleAction.PauseAction)
            browserStore.dispatch(AppLifecycleAction.PauseAction)
        }
    }

    @Test
    fun `WHEN onResume is called THEN dispatch ResumeAction`() {
        val appStore: AppStore = mockk(relaxed = true)
        val browserStore: BrowserStore = mockk(relaxed = true)
        val observer = StoreLifecycleObserver(appStore, browserStore)

        observer.onResume(mockk())

        verifySequence {
            appStore.dispatch(AppAction.AppLifecycleAction.ResumeAction)
            browserStore.dispatch(AppLifecycleAction.ResumeAction)
        }
    }
}
