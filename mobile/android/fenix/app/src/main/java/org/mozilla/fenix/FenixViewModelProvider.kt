/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.mvi.ViewState

object FenixViewModelProvider {
    fun <S : ViewState, C : Change, T : UIComponentViewModelBase<S, C>> create(
        fragment: Fragment,
        modelClass: Class<T>,
        viewModelCreator: () -> T
    ): UIComponentViewModelProvider<S, C> {
        val factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel?> create(modelClass: Class<T>): T {
                return viewModelCreator() as T
            }
        }

        return object : UIComponentViewModelProvider<S, C> {
            override fun fetchViewModel(): T {
                return ViewModelProvider(fragment, factory).get(modelClass)
            }
        }
    }
}
