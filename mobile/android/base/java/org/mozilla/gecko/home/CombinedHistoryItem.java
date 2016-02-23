/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.support.v7.widget.RecyclerView;
import android.view.View;

public abstract class CombinedHistoryItem extends RecyclerView.ViewHolder {
    public CombinedHistoryItem(View view) {
        super(view);
    }

    public static class HistoryItem extends CombinedHistoryItem {
        public HistoryItem(View view) {
            super(view);
        }
    }

    public static class ClientItem extends CombinedHistoryItem {
        public ClientItem(View view) {
            super(view);
        }

    }
}
