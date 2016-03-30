/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

public class CombinedHistoryRecyclerView extends RecyclerView
        implements RecyclerViewClickSupport.OnItemClickListener, RecyclerViewClickSupport.OnItemLongClickListener {

    public CombinedHistoryRecyclerView(Context context) {
        super(context);
        init(context);
    }

    public CombinedHistoryRecyclerView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        init(context);
    }

    public CombinedHistoryRecyclerView(Context context, AttributeSet attributeSet, int defStyle) {
        super(context, attributeSet, defStyle);
        init(context);
    }

    private void init(Context context) {
        LinearLayoutManager layoutManager = new LinearLayoutManager(context);
        layoutManager.setOrientation(LinearLayoutManager.VERTICAL);
        setLayoutManager(layoutManager);
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {}

    @Override
    public boolean onItemLongClicked(RecyclerView recyclerView, int position, View v) {
        // TODO: open context menu if not a date title
        return showContextMenuForChild(this);
    }
}
