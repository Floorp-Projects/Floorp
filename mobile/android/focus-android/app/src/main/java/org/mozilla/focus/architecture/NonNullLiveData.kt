/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.architecture

import android.arch.lifecycle.LiveData

/**
 * A LiveData implementation with an initial value and that does not allow null values.
 */
open class NonNullLiveData<T>(initialValue: T) : LiveData<T>() {
    init {
        value = initialValue
    }

    /**
     * Returns the current (non-null) value. Note that calling this method on a background thread
     * does not guarantee that the latest value set will be received.
     */
    override fun getValue(): T = super.getValue() ?: throw IllegalStateException("Value is null")

    /**
     * Posts a task to a main thread to set the given (non null) value.
     */
    override fun postValue(value: T?) {
        if (value == null) {
            throw IllegalArgumentException("Value cannot be null")
        }
        super.postValue(value)
    }

    /**
     * Sets the (non-null) value. If there are active observers, the value will be dispatched to them.
     */
    override fun setValue(value: T?) {
        if (value == null) {
            throw IllegalArgumentException("Value cannot be null")
        }
        super.setValue(value)
    }
}
