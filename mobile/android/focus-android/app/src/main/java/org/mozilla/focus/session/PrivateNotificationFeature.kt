/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Responsible for starting or stopping up a [SessionNotificationService]
 * depending on whether a private tab is opened.
 *
 * @param browserStore Browser store reference used to observe the number of private tabs.
 */
class PrivateNotificationFeature(
    context: Context,
    private val browserStore: BrowserStore,
    private val permissionRequestHandler: (() -> Unit),
) : LifecycleAwareFeature {

    private val applicationContext = context.applicationContext
    private var scope: CoroutineScope? = null

    override fun start() {
        scope = browserStore.flowScoped { flow ->
            flow.map { state -> state.privateTabs.isNotEmpty() }
                .distinctUntilChanged()
                .collect { hasPrivateTabs ->
                    if (hasPrivateTabs) {
                        SessionNotificationService.start(applicationContext, permissionRequestHandler)
                    } else {
                        SessionNotificationService.stop(applicationContext)
                    }
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
