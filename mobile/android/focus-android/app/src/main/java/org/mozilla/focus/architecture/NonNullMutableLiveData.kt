/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.architecture

import android.support.annotation.NonNull

class NonNullMutableLiveData<T>(initialValue: T) : NonNullLiveData<T>(initialValue) {
    /**
     * Posts a task to a main thread to set the given (non-null) value.
     */
    public override fun postValue(value: T?) {
        super.postValue(value)
    }

    /**
     * Sets the (non-null) value. If there are active observers, the value will be dispatched to them.
     */
    public override fun setValue(value: T?) {
        super.setValue(value)
    }

    @NonNull
    override fun getValue(): T = super.getValue()
}
