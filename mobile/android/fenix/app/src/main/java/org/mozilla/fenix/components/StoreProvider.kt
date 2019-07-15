/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelProviders
import androidx.lifecycle.get
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Generic ViewModel to wrap a State object for state restoration
 */
@Suppress("UNCHECKED_CAST")
class StoreProvider<S : State, A : Action, T : Store<S, A>>(val store: T) : ViewModel() {
    companion object {
        fun <S : State, A : Action, T : Store<S, A>> get(fragment: Fragment, initialStore: T): T {
            val factory = object : ViewModelProvider.Factory {
                override fun <VM : ViewModel?> create(modelClass: Class<VM>): VM {
                    return StoreProvider(initialStore) as VM
                }
            }

            val viewModel: StoreProvider<S, A, T> = ViewModelProviders.of(fragment, factory).get()
            return viewModel.store
        }
    }
}
