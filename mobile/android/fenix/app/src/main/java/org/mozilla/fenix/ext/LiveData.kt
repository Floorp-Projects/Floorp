/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.lifecycle.LiveData
import androidx.lifecycle.Observer

/**
 * Observe a LiveData once and unregister from it as soon as the live data returns a value
 */
fun <T> LiveData<T>.observeOnceAndRemoveObserver(callback: (T) -> Unit) {
    observeForever(object : Observer<T> {
        override fun onChanged(value: T) {
            removeObserver(this)
            callback(value)
        }
    })
}
