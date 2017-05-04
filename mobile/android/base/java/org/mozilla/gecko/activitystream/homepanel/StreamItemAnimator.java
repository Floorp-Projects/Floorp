/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.activitystream.homepanel;

import android.support.annotation.NonNull;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.RecyclerView;

import org.mozilla.gecko.activitystream.homepanel.stream.TopPanel;

/**
 * We need our own item animator override to avoid default RV-style animations for certain panels.
 */
public class StreamItemAnimator extends DefaultItemAnimator {

    @Override
    public boolean canReuseUpdatedViewHolder(@NonNull RecyclerView.ViewHolder viewHolder) {
        if (viewHolder.getItemViewType() == TopPanel.LAYOUT_ID) {
            // The top panel doesn't ever change in size. We really don't want to reload it
            // because it has its own state (i.e. the ViewPager state gets lost, and we
            // also flicker when creating a new ViewHolder). Therefore we should try to
            // keep the existing TopPanel whenever possible.
            return true;
        }

        return super.canReuseUpdatedViewHolder(viewHolder);
    }
}
