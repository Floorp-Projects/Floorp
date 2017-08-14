/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.architecture;

import android.arch.lifecycle.Observer;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

public abstract class NonNullObserver<T> implements Observer<T> {
    protected abstract void onValueChanged(@NonNull T t);

    @Override
    public final void onChanged(@Nullable T value) {
        if (value == null) {
            return;
        }

        onValueChanged(value);
    }
}
