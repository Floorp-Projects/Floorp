/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.support.v4.app.Fragment;

public class FirstrunPanel extends Fragment {

    protected FirstrunPane.OnFinishListener onFinishListener;

    public void setOnFinishListener(FirstrunPane.OnFinishListener listener) {
        this.onFinishListener = listener;
    }

    protected void close() {
        if (onFinishListener != null) {
            onFinishListener.onFinish();
        }
    }
}
