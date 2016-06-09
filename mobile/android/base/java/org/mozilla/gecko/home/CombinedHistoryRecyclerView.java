/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.home.CombinedHistoryPanel.OnPanelLevelChangeListener;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import java.util.EnumSet;

import static org.mozilla.gecko.home.CombinedHistoryPanel.OnPanelLevelChangeListener.PanelLevel.CHILD_RECENT_TABS;
import static org.mozilla.gecko.home.CombinedHistoryPanel.OnPanelLevelChangeListener.PanelLevel.CHILD_SYNC;
import static org.mozilla.gecko.home.CombinedHistoryPanel.OnPanelLevelChangeListener.PanelLevel.PARENT;

public class CombinedHistoryRecyclerView extends RecyclerView
        implements RecyclerViewClickSupport.OnItemClickListener, RecyclerViewClickSupport.OnItemLongClickListener {
    public static String LOGTAG = "CombinedHistoryRecycView";

    protected interface AdapterContextMenuBuilder {
        HomeContextMenuInfo makeContextMenuInfoFromPosition(View view, int position);
    }

    protected HomePager.OnUrlOpenListener mOnUrlOpenListener;
    protected OnPanelLevelChangeListener mOnPanelLevelChangeListener;
    protected CombinedHistoryPanel.DialogBuilder<RemoteClient> mDialogBuilder;
    protected HomeContextMenuInfo mContextMenuInfo;

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

        RecyclerViewClickSupport.addTo(this)
            .setOnItemClickListener(this)
            .setOnItemLongClickListener(this);

        setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                final int action = event.getAction();

                // If the user hit the BACK key, try to move to the parent folder.
                if (action == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
                    return mOnPanelLevelChangeListener.changeLevel(PARENT);
                }
                return false;
            }
        });
    }

    public void setOnHistoryClickedListener(HomePager.OnUrlOpenListener listener) {
        this.mOnUrlOpenListener = listener;
    }

    public void setOnPanelLevelChangeListener(OnPanelLevelChangeListener listener) {
        this.mOnPanelLevelChangeListener = listener;
    }

    public void setHiddenClientsDialogBuilder(CombinedHistoryPanel.DialogBuilder<RemoteClient> builder) {
        mDialogBuilder = builder;
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        final int viewType = getAdapter().getItemViewType(position);
        final CombinedHistoryItem.ItemType itemType = CombinedHistoryItem.ItemType.viewTypeToItemType(viewType);
        final String telemetryExtra;

        switch (itemType) {
            case RECENT_TABS:
                mOnPanelLevelChangeListener.changeLevel(CHILD_RECENT_TABS);
                break;

            case SYNCED_DEVICES:
                mOnPanelLevelChangeListener.changeLevel(CHILD_SYNC);
                break;

            case CLIENT:
                ((ClientsAdapter) getAdapter()).toggleClient(position);
                break;

            case HIDDEN_DEVICES:
                if (mDialogBuilder != null) {
                    mDialogBuilder.createAndShowDialog(((ClientsAdapter) getAdapter()).getHiddenClients());
                }
                break;

            case NAVIGATION_BACK:
                mOnPanelLevelChangeListener.changeLevel(PARENT);
                break;

            case CHILD:
            case HISTORY:
                if (mOnUrlOpenListener != null) {
                    final TwoLinePageRow historyItem = (TwoLinePageRow) v;
                    Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, "history");
                    mOnUrlOpenListener.onUrlOpen(historyItem.getUrl(), EnumSet.of(HomePager.OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
                }
                break;

            case CLOSED_TAB:
                telemetryExtra = ((RecentTabsAdapter) getAdapter()).restoreTabFromPosition(position);
                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, telemetryExtra);
                break;
        }
    }

    @Override
    public boolean onItemLongClicked(RecyclerView recyclerView, int position, View v) {
        mContextMenuInfo = ((AdapterContextMenuBuilder) getAdapter()).makeContextMenuInfoFromPosition(v, position);
        return showContextMenuForChild(this);
    }

    @Override
    public HomeContextMenuInfo getContextMenuInfo() {
        return mContextMenuInfo;
    }

}
