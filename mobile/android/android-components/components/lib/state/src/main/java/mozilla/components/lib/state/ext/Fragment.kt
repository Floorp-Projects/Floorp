/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.view.View
import androidx.annotation.MainThread
import androidx.fragment.app.Fragment
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.support.ktx.android.view.toScope

/**
 * Helper extension method for consuming [State] from a [Store] sequentially in order inside a
 * [Fragment]. The [block] function will get invoked for every [State] update.
 *
 * This helper will automatically stop observing the [Store] once the [View] of the [Fragment] gets
 * detached. The fragment's lifecycle will be used to determine when to resume/pause observing the
 * [Store].
 */
@MainThread
@ExperimentalCoroutinesApi // Channel
@ObsoleteCoroutinesApi // consumeEach
fun <S : State, A : Action> Fragment.consumeFrom(store: Store<S, A>, block: (S) -> Unit) {
    val view = checkNotNull(view) { "Fragment has no view yet. Call from onViewCreated()." }

    val scope = view.toScope()
    val channel = store.broadcastChannel(owner = this)

    scope.launch {
        channel.consumeEach { state -> block(state) }
    }
}
