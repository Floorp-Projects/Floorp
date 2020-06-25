/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.ext.components

/**
 * This observer starts and stops the service to show a notification
 * indicating that a private tab is open.
 */
class NotificationSessionObserver(
    private val context: Context,
    private val notificationService: SessionNotificationService.Companion = SessionNotificationService
) {

    private var scope: CoroutineScope? = null
    private var started = false

    @ExperimentalCoroutinesApi
    fun start() {
        scope = context.components.core.store.flowScoped { flow ->
            flow.map { state -> state.privateTabs.isNotEmpty() }
                .ifChanged()
                .collect { hasPrivateTabs ->
                    if (hasPrivateTabs) {
                        notificationService.start(context, isStartedFromPrivateShortcut)
                        started = true
                    } else if (started) {
                        notificationService.stop(context)
                        started = false
                    }
                }
        }
    }

    fun stop() {
        scope?.cancel()
    }

    companion object {
        var isStartedFromPrivateShortcut = false
    }
}
