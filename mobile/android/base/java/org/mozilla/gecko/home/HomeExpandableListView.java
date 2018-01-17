/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.ExpandableListView;

/**
 * <code>HomeExpandableListView</code> is a custom extension of
 * <code>ExpandableListView<code>, that packs a <code>HomeContextMenuInfo</code>
 * when any of its rows is long pressed.
 * <p>
 * This is the <code>ExpandableListView</code> equivalent of
 * <code>HomeListView</code>.
 */
public class HomeExpandableListView extends ExpandableListView
                                    implements OnItemLongClickListener {

    // ContextMenuInfo associated with the currently long pressed list item.
    private HomeContextMenuInfo mContextMenuInfo;

    // ContextMenuInfo factory.
    private HomeContextMenuInfo.ExpandableFactory mContextMenuInfoFactory;

    public HomeExpandableListView(Context context) {
        this(context, null);
    }

    public HomeExpandableListView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public HomeExpandableListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        setOnItemLongClickListener(this);
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        if (mContextMenuInfoFactory == null) {
            return false;
        }

        // HomeExpandableListView items can correspond to groups and children.
        // The factory can determine whether to add context menu for either,
        // both, or none by unpacking the given position.
        mContextMenuInfo = mContextMenuInfoFactory.makeInfoForAdapter(view, position, id, getExpandableListAdapter());
        return showContextMenuForChild(HomeExpandableListView.this);
    }

    @Override
    public ContextMenuInfo getContextMenuInfo() {
        return mContextMenuInfo;
    }

    public void setContextMenuInfoFactory(final HomeContextMenuInfo.ExpandableFactory factory) {
        mContextMenuInfoFactory = factory;
    }
}
