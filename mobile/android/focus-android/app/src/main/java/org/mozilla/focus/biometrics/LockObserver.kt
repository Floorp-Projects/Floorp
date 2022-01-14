/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.content.Context
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.focus.GleanMetrics.TabCount
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore

class LockObserver(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val appStore: AppStore
) : DefaultLifecycleObserver {

    override fun onPause(owner: LifecycleOwner) {
        super.onPause(owner)

        val tabCount = browserStore.state.privateTabs.size.toLong()
        TabCount.appBackgrounded.accumulateSamples(longArrayOf(tabCount))

        if (tabCount == 0L) {
            return
        }

        if (Biometrics.isBiometricsEnabled(context)) {
            appStore.dispatch(AppAction.Lock)
        }
    }
}
