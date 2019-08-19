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
    val fragment = this
    val view = checkNotNull(view) { "Fragment has no view yet. Call from onViewCreated()." }

    val scope = view.toScope()
    val channel = store.channel(owner = this)

    scope.launch {
        channel.consumeEach { state ->
            // We are using a scope that is bound to the view being attached here. It can happen
            // that the "view detached" callback gets executed *after* the fragment was detached. If
            // a `consumeFrom` runs in exactly this moment then we run inside a detached fragment
            // without a `Context` and this can cause a variety of issues/crashes.
            // See: https://github.com/mozilla-mobile/android-components/issues/4125
            //
            // To avoid this, we check whether the fragment is added. If it's not added then we run
            // in exactly that moment between fragment detach and view detach.
            // It would be better if we could use `viewLifecycleOwner` which is bound to
            // onCreateView() and onDestroyView() of the fragment. But:
            // - `viewLifecycleOwner` is only available in alpha versions of AndroidX currently.
            // - We found a bug where `viewLifecycleOwner.lifecycleScope` is not getting cancelled
            //   causing this coroutine to run forever.
            //   See: https://github.com/mozilla-mobile/android-components/issues/3828
            // Once those two issues get resolved we can remove the `isAdded` check and use
            // `viewLifecycleOwner.lifecycleScope` instead of the view scope.
            if (fragment.isAdded) {
                block(state)
            }
        }
    }
}
