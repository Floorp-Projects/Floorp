/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelProviders
import androidx.lifecycle.get
import mozilla.components.lib.state.State

/**
 * Generic ViewModel to wrap a State object for state restoration
 */
@Suppress("UNCHECKED_CAST")
class StateViewModel<T : State>(initialState: T) : ViewModel() {
    var state: T = initialState
        private set(value) { field = value }

    fun update(state: T) { this.state = state }

    companion object {
        fun <S : State> get(fragment: Fragment, initialState: S): StateViewModel<S> {
            val factory = object : ViewModelProvider.Factory {
                override fun <T : ViewModel?> create(modelClass: Class<T>): T {
                    return StateViewModel(initialState) as T
                }
            }

            return ViewModelProviders.of(fragment, factory).get()
        }
    }
}
