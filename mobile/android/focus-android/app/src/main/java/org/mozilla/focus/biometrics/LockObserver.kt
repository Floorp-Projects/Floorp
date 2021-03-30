/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.content.Context
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore

class LockObserver(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val appStore: AppStore
) : LifecycleObserver {
    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onPause() {
        if (browserStore.state.privateTabs.isEmpty()) {
            return
        }

        if (Biometrics.isBiometricsEnabled(context)) {
            appStore.dispatch(AppAction.Lock)
        }
    }
}
