/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.architecture;

public class NonNullMutableLiveData<T> extends NonNullLiveData<T> {
    public NonNullMutableLiveData(T initialValue) {
        super(initialValue);
    }

    /**
     * Posts a task to a main thread to set the given (non-null) value.
     */
    @Override
    public void postValue(T value) {
        super.postValue(value);
    }

    /**
     * Sets the (non-null) value. If there are active observers, the value will be dispatched to them.
     */
    @Override
    public void setValue(T value) {
        super.setValue(value);
    }
}
