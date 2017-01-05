/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home.activitystream.stream;

import android.support.v7.widget.RecyclerView;
import android.view.View;

public abstract class StreamItem extends RecyclerView.ViewHolder {
    public StreamItem(View itemView) {
        super(itemView);
    }
}
