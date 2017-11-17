/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.architecture

import android.arch.lifecycle.Observer

abstract class NonNullObserver<T> : Observer<T> {
    protected abstract fun onValueChanged(t: T)

    override fun onChanged(value: T?) {
        value?.let {
            onValueChanged(it)
        }
    }
}
