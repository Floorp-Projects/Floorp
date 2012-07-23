/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.HashMap;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.View;
import android.widget.PopupWindow;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

public class DoorHangerPopup extends PopupWindow {
    private static final String LOGTAG = "GeckoDoorHangerPopup";

    private Context mContext;
    private LinearLayout mContent;

    private boolean mInflated; 
    private ImageView mArrow;
    private int mArrowWidth;

    public DoorHangerPopup(Context aContext) {
        super(aContext);
        mContext = aContext;

        mInflated = false;
        mArrowWidth = aContext.getResources().getDimensionPixelSize(R.dimen.doorhanger_arrow_width);
   }

    private void init() {
        setBackgroundDrawable(new BitmapDrawable());
        setOutsideTouchable(true);
        setFocusable(true);
        setWindowLayoutMode(GeckoApp.mAppContext.isTablet() ? ViewGroup.LayoutParams.WRAP_CONTENT : ViewGroup.LayoutParams.FILL_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(mContext);
        RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.doorhangerpopup, null);
        mArrow = (ImageView) layout.findViewById(R.id.doorhanger_arrow);
        mContent = (LinearLayout) layout.findViewById(R.id.doorhanger_container);
        
        setContentView(layout);
        mInflated = true;
    }

    public void addDoorHanger(String message, String value, JSONArray buttons,
                              Tab tab, JSONObject options, View v) {
        Log.i(LOGTAG, "Adding a DoorHanger to Tab: " + tab.getId());

        if (!mInflated)
            init();

        // Replace the doorhanger if it already exists
        DoorHanger dh = tab.getDoorHanger(value);
        if (dh != null) {
            tab.removeDoorHanger(value);
        }
        dh = new DoorHanger(mContent.getContext(), value);
 
        // Set the doorhanger text and buttons
        dh.setText(message);
        for (int i = 0; i < buttons.length(); i++) {
            try {
                JSONObject buttonObject = buttons.getJSONObject(i);
                String label = buttonObject.getString("label");
                int callBackId = buttonObject.getInt("callback");
                dh.addButton(label, callBackId);
            } catch (JSONException e) {
                Log.i(LOGTAG, "JSON throws", e);
            }
         }
        dh.setOptions(options);

        dh.setTab(tab);
        tab.addDoorHanger(value, dh);
        mContent.addView(dh);

        // Only update the popup if we're adding a notifcation to the selected tab
        if (tab.equals(Tabs.getInstance().getSelectedTab()))
            updatePopup(v);
    }

    // Updates popup contents to show doorhangers for the selected tab
    public void updatePopup() {
      updatePopup(null);
    }

    public void updatePopup(View v) {
        Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab == null) {
            hidePopup();
            return;
        }
        
        Log.i(LOGTAG, "Showing all doorhangers for tab: " + tab.getId());
 
        HashMap<String, DoorHanger> doorHangers = tab.getDoorHangers();
        // Hide the popup if there aren't any doorhangers to show
        if (doorHangers == null || doorHangers.size() == 0) {
            hidePopup();
            return;
        }

        if (!mInflated)
            init();

        // Hide old doorhangers
        for (int i = 0; i < mContent.getChildCount(); i++) {
            DoorHanger dh = (DoorHanger) mContent.getChildAt(i);
            dh.hide();
        }

        // Show the doorhangers for the tab
        for (DoorHanger dh : doorHangers.values()) {
            dh.show();
        }

        if (v == null)
            showAtLocation(((GeckoApp)mContext).getView(), Gravity.TOP, 0, 0);
        else
            showPopup(v);
    }

    public void hidePopup() {
        if (isShowing()) {
            dismiss();
        }
    }

    public void showPopup(View v) {
        fixBackgroundForFirst();

        if (isShowing()) {
            update();
            return;
        }

        // On tablets, we need to position the popup so that the center of the arrow points to the
        // center of the anchor view. On phones the popup stretches across the entire screen, so the
        // arrow position is determined by its left margin.
        int offset = GeckoApp.mAppContext.isTablet() ? v.getWidth()/2 - mArrowWidth/2 -
                     ((RelativeLayout.LayoutParams) mArrow.getLayoutParams()).leftMargin : 0;

        showAsDropDown(v, offset, 0);
    }

    private void fixBackgroundForFirst() {
        for (int i=0; i < mContent.getChildCount(); i++) {
            DoorHanger dh = (DoorHanger) mContent.getChildAt(i);
            if (dh.isVisible()) {
                dh.setBackgroundResource(R.drawable.doorhanger_bg);
                break;
            }
        }
    }
}
