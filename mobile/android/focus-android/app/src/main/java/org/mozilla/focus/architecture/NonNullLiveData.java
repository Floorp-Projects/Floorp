/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.architecture;

import android.arch.lifecycle.LiveData;
import android.support.annotation.NonNull;

public class NonNullLiveData<T> extends LiveData<T> {
    public NonNullLiveData(final T initialValue) {
        setValue(initialValue);
    }

    /**
     * Returns the current (non-null) value. Note that calling this method on a background thread
     * does not guarantee that the latest value set will be received.
     */
    @NonNull
    public T getValue() {
        final T value = super.getValue();
        if (value == null) {
            throw new IllegalStateException("Value is null");
        }
        return value;
    }

    /**
     * Posts a task to a main thread to set the given (non null) value.
     */
    @Override
    protected void postValue(T value) {
        if (value == null) {
            throw new IllegalArgumentException("Value cannot be null");
        }
        super.postValue(value);
    }

    /**
     * Sets the (non-null) value. If there are active observers, the value will be dispatched to them.
     */
    @Override
    protected void setValue(T value) {
        if (value == null) {
            throw new IllegalArgumentException("Value cannot be null");
        }
        super.setValue(value);
    }
}
