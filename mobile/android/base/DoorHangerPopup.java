/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;

import java.util.HashSet;

public class DoorHangerPopup extends PopupWindow
                             implements GeckoEventListener, Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "GeckoDoorHangerPopup";

    private GeckoApp mActivity;
    private View mAnchor;
    private LinearLayout mContent;

    private boolean mInflated; 
    private ImageView mArrow;
    private int mArrowWidth;

    // Stores a set of all active DoorHanger notifications. A DoorHanger is
    // uniquely identified by its tabId and value.
    private HashSet<DoorHanger> mDoorHangers;

    DoorHangerPopup(GeckoApp aActivity, View aAnchor) {
        super(aActivity);
        mActivity = aActivity;
        mAnchor = aAnchor;

        mInflated = false;
        mArrowWidth = aActivity.getResources().getDimensionPixelSize(R.dimen.doorhanger_arrow_width);
        mDoorHangers = new HashSet<DoorHanger>();

        GeckoAppShell.registerGeckoEventListener("Doorhanger:Add", this);
        GeckoAppShell.registerGeckoEventListener("Doorhanger:Remove", this);
        Tabs.registerOnTabsChangedListener(this);
    }

    void destroy() {
        GeckoAppShell.unregisterGeckoEventListener("Doorhanger:Add", this);
        GeckoAppShell.unregisterGeckoEventListener("Doorhanger:Remove", this);
        Tabs.unregisterOnTabsChangedListener(this);
    }

    public void handleMessage(String event, JSONObject geckoObject) {
        try {
            if (event.equals("Doorhanger:Add")) {
                int tabId = geckoObject.getInt("tabID");
                String value = geckoObject.getString("value");
                String message = geckoObject.getString("message");
                JSONArray buttons = geckoObject.getJSONArray("buttons");
                JSONObject options = geckoObject.getJSONObject("options");

                addDoorHanger(tabId, value, message, buttons, options);
            } else if (event.equals("Doorhanger:Remove")) {
                int tabId = geckoObject.getInt("tabID");
                String value = geckoObject.getString("value");
                DoorHanger doorHanger = getDoorHanger(tabId, value);
                if (doorHanger == null)
                    return;

                removeDoorHanger(doorHanger);
                mActivity.runOnUiThread(new Runnable() {
                    public void run() {
                        updatePopup();
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch(msg) {
            case CLOSED:
                // Remove any doorhangers for a tab when it's closed
                for (DoorHanger dh : mDoorHangers) {
                    if (dh.getTabId() == tab.getId())
                        removeDoorHanger(dh);
                }
                break;

            case LOCATION_CHANGE:
                // Only remove doorhangers if the popup is hidden or if we're navigating to a new URL
                if (!isShowing() || !data.equals(tab.getURL()))
                    removeTransientDoorHangers(tab.getId());

                // Update the popup if the location change was on the current tab
                if (Tabs.getInstance().isSelectedTab(tab))
                    updatePopup();
                break;

            case SELECTED:
                // Always update the popup when a new tab is selected. This will cover cases
                // where a different tab was closed, since we always need to select a new tab.
                updatePopup();
                break;
        }
    }

    private void init() {
        setBackgroundDrawable(new BitmapDrawable());
        setOutsideTouchable(true);
        setFocusable(true);
        setWindowLayoutMode(mActivity.isTablet() ? ViewGroup.LayoutParams.WRAP_CONTENT : ViewGroup.LayoutParams.FILL_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.doorhangerpopup, null);
        mArrow = (ImageView) layout.findViewById(R.id.doorhanger_arrow);
        mContent = (LinearLayout) layout.findViewById(R.id.doorhanger_container);
        
        setContentView(layout);
        mInflated = true;
    }

    void addDoorHanger(final int tabId, final String value, final String message,
                       final JSONArray buttons, final JSONObject options) {
        // Don't add a doorhanger for a tab that doesn't exist
        if (Tabs.getInstance().getTab(tabId) == null)
            return;

        // Replace the doorhanger if it already exists
        DoorHanger oldDoorHanger = getDoorHanger(tabId, value);
        if (oldDoorHanger != null)
            removeDoorHanger(oldDoorHanger);

        final DoorHanger newDoorHanger = new DoorHanger(mActivity, this, tabId, value);
        mDoorHangers.add(newDoorHanger);

        // Update the UI bits on the main thread
        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                if (!mInflated)
                    init();

                newDoorHanger.init(message, buttons, options);
                mContent.addView(newDoorHanger);

                // Only update the popup if we're adding a notifcation to the selected tab
                if (tabId == Tabs.getInstance().getSelectedTab().getId())
                    updatePopup();
            }
        });
    }

    DoorHanger getDoorHanger(int tabId, String value) {
        for (DoorHanger dh : mDoorHangers) {
            if (dh.getTabId() == tabId && dh.getValue().equals(value))
                return dh;
        }

        // If there's no doorhanger for the given tabId and value, return null
        return null;
    }

    void removeDoorHanger(final DoorHanger doorHanger) {
        mDoorHangers.remove(doorHanger);

        // Update the UI on the main thread
        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                mContent.removeView(doorHanger);
            }
        });        
    }

    void removeTransientDoorHangers(int tabId) {
        // Make a temporary set to avoid a ConcurrentModificationException
        HashSet<DoorHanger> doorHangersToRemove = new HashSet<DoorHanger>();
        for (DoorHanger dh : mDoorHangers) {
            // Only remove transient doorhangers for the given tab
            if (dh.getTabId() == tabId && dh.shouldRemove())
                doorHangersToRemove.add(dh);
        }

        for (DoorHanger dh : doorHangersToRemove) {
            removeDoorHanger(dh);
        }
    }

    void updatePopup() {
        // Bail if the selected tab is null, if there are no active doorhangers,
        // or if we haven't inflated the layout yet (this can happen if updatePopup()
        // is called before the runnable from addDoorHanger() runs). 
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null || mDoorHangers.size() == 0 || !mInflated) {
            dismiss();
            return;
        }

        // Show doorhangers for the selected tab
        int tabId = tab.getId();
        boolean shouldShowPopup = false;
        for (DoorHanger dh : mDoorHangers) {
            if (dh.getTabId() == tabId) {
                dh.setVisibility(View.VISIBLE);
                shouldShowPopup = true;
            } else {
                dh.setVisibility(View.GONE);
            }
        }
 
        // Dismiss the popup if there are no doorhangers to show for this tab
        if (!shouldShowPopup) {
            dismiss();
            return;
        }

        fixBackgroundForFirst();
        if (isShowing()) {
            update();
            return;
        }

        // If there's no anchor, just show the popup at the top of the gecko app view.
        if (mAnchor == null) {
            showAtLocation(mActivity.getView(), Gravity.TOP, 0, 0);
            return;
        }

        // On tablets, we need to position the popup so that the center of the arrow points to the
        // center of the anchor view. On phones the popup stretches across the entire screen, so the
        // arrow position is determined by its left margin.
        int offset = mActivity.isTablet() ? mAnchor.getWidth()/2 - mArrowWidth/2 -
                     ((RelativeLayout.LayoutParams) mArrow.getLayoutParams()).leftMargin : 0;
        showAsDropDown(mAnchor, offset, 0);
    }

    private void fixBackgroundForFirst() {
        for (int i = 0; i < mContent.getChildCount(); i++) {
            DoorHanger dh = (DoorHanger) mContent.getChildAt(i);
            if (dh.getVisibility() == View.VISIBLE) {
                dh.setBackgroundResource(R.drawable.doorhanger_bg);
                break;
            }
        }
    }
}
