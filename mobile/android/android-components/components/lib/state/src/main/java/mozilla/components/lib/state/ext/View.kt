/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.view.View
import androidx.fragment.app.Fragment
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.support.ktx.android.view.toScope

/**
 * Helper extension method for consuming [State] from a [Store] sequentially in order scoped to the
 * lifetime of the [View]. The [block] function will get invoked for every [State] update.
 *
 * This helper will automatically stop observing the [Store] once the [View] gets detached. The
 * provided [LifecycleOwner] is used to determine when observing should be stopped or resumed.
 *
 * Inside a [Fragment] prefer to use [Fragment.consumeFrom].
 */
@ExperimentalCoroutinesApi // Channel
fun <S : State, A : Action> View.consumeFrom(
    store: Store<S, A>,
    owner: LifecycleOwner,
    block: (S) -> Unit,
) {
    val scope = toScope()
    val channel = store.channel(owner)

    scope.launch {
        channel.consumeEach { state -> block(state) }
    }
}
