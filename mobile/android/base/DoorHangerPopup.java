/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.HardwareUtils;

import org.json.JSONArray;
import org.json.JSONObject;

import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
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
    private int mYOffset;

    // Stores a set of all active DoorHanger notifications. A DoorHanger is
    // uniquely identified by its tabId and value.
    private HashSet<DoorHanger> mDoorHangers;

    DoorHangerPopup(GeckoApp aActivity, View aAnchor) {
        super(aActivity);
        mActivity = aActivity;
        mAnchor = aAnchor;

        mInflated = false;
        mArrowWidth = aActivity.getResources().getDimensionPixelSize(R.dimen.menu_popup_arrow_width);
        mYOffset = aActivity.getResources().getDimensionPixelSize(R.dimen.menu_popup_offset);
        mDoorHangers = new HashSet<DoorHanger>();

        registerEventListener("Doorhanger:Add");
        registerEventListener("Doorhanger:Remove");
        Tabs.registerOnTabsChangedListener(this);

        setAnimationStyle(R.style.PopupAnimation);
    }

    void destroy() {
        unregisterEventListener("Doorhanger:Add");
        unregisterEventListener("Doorhanger:Remove");
        Tabs.unregisterOnTabsChangedListener(this);
    }

    void setAnchor(View aAnchor) {
        mAnchor = aAnchor;
    }

    @Override
    public void handleMessage(String event, JSONObject geckoObject) {
        try {
            if (event.equals("Doorhanger:Add")) {
                final int tabId = geckoObject.getInt("tabID");
                final String value = geckoObject.getString("value");
                final String message = geckoObject.getString("message");
                final JSONArray buttons = geckoObject.getJSONArray("buttons");
                final JSONObject options = geckoObject.getJSONObject("options");

                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        addDoorHanger(tabId, value, message, buttons, options);
                    }
                });
            } else if (event.equals("Doorhanger:Remove")) {
                final int tabId = geckoObject.getInt("tabID");
                final String value = geckoObject.getString("value");

                mActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        DoorHanger doorHanger = getDoorHanger(tabId, value);
                        if (doorHanger == null)
                            return;

                        removeDoorHanger(doorHanger);
                        updatePopup();
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    // This callback is automatically executed on the UI thread.
    @Override
    public void onTabChanged(final Tab tab, final Tabs.TabEvents msg, final Object data) {
        switch(msg) {
            case CLOSED:
                // Remove any doorhangers for a tab when it's closed (make
                // a temporary set to avoid a ConcurrentModificationException)
                HashSet<DoorHanger> doorHangersToRemove = new HashSet<DoorHanger>();
                for (DoorHanger dh : mDoorHangers) {
                    if (dh.getTabId() == tab.getId())
                        doorHangersToRemove.add(dh);
                }
                for (DoorHanger dh : doorHangersToRemove) {
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
        setWindowLayoutMode(HardwareUtils.isTablet() ? ViewGroup.LayoutParams.WRAP_CONTENT : ViewGroup.LayoutParams.FILL_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.doorhangerpopup, null);
        mArrow = (ImageView) layout.findViewById(R.id.doorhanger_arrow);
        mContent = (LinearLayout) layout.findViewById(R.id.doorhanger_container);
        
        setContentView(layout);
        mInflated = true;
    }

    /**
     * Adds a doorhanger.
     *
     * This method must be called on the UI thread.
     */
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

        if (!mInflated)
            init();

        newDoorHanger.init(message, buttons, options);
        mContent.addView(newDoorHanger);

        // Only update the popup if we're adding a notifcation to the selected tab
        if (tabId == Tabs.getInstance().getSelectedTab().getId())
            updatePopup();
    }

    /**
     * Gets a doorhanger.
     *
     * This method must be called on the UI thread.
     */
    DoorHanger getDoorHanger(int tabId, String value) {
        for (DoorHanger dh : mDoorHangers) {
            if (dh.getTabId() == tabId && dh.getValue().equals(value))
                return dh;
        }

        // If there's no doorhanger for the given tabId and value, return null
        return null;
    }

    /**
     * Removes a doorhanger.
     *
     * This method must be called on the UI thread.
     */
    void removeDoorHanger(final DoorHanger doorHanger) {
        mDoorHangers.remove(doorHanger);
        mContent.removeView(doorHanger);
    }

    /**
     * Removes doorhangers for a given tab.
     *
     * This method must be called on the UI thread.
     */
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

    /**
     * Updates the popup state.
     *
     * This method must be called on the UI thread.
     */
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

        showDividers();
        if (isShowing()) {
            update();
            return;
        }

        int[] anchorLocation = new int[2];
        if (mAnchor != null)
            mAnchor.getLocationInWindow(anchorLocation);

        // Make the popup focusable for accessibility. This gets done here
        // so the node can be accessibility focused, but on pre-ICS devices this
        // causes crashes, so it is done after the popup is shown.
        if (Build.VERSION.SDK_INT >= 14) {
            setFocusable(true);
        }

        // If there's no anchor or the anchor is out of the window bounds,
        // just show the popup at the top of the gecko app view.
        if (mAnchor == null || anchorLocation[1] < 0) {
            showAtLocation(mActivity.getView(), Gravity.TOP, 0, 0);
        } else {
            // On tablets, we need to position the popup so that the center of the arrow points to the
            // center of the anchor view. On phones the popup stretches across the entire screen, so the
            // arrow position is determined by its left margin.
            int offset = HardwareUtils.isTablet() ? mAnchor.getWidth()/2 - mArrowWidth/2 -
                         ((RelativeLayout.LayoutParams) mArrow.getLayoutParams()).leftMargin : 0;
            showAsDropDown(mAnchor, offset, -mYOffset);
        }

        if (Build.VERSION.SDK_INT < 14) {
            // Make the popup focusable for keyboard accessibility.
            setFocusable(true);
        }
    }

    private void showDividers() {
        int count = mContent.getChildCount();

        for (int i = 0; i < count; i++) {
            DoorHanger dh = (DoorHanger) mContent.getChildAt(i);
            dh.showDivider();
        }

        ((DoorHanger) mContent.getChildAt(count-1)).hideDivider();
    }

    private void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    private void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
    }

    @Override
    public void dismiss() {
        // If the popup is focusable while it is hidden, we run into crashes
        // on pre-ICS devices when the popup gets focus before it is shown.
        setFocusable(false);
        super.dismiss();
    }
}
