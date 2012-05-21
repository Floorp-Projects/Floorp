/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.HashMap;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.PopupWindow;
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

    public DoorHangerPopup(Context aContext) {
        super(aContext);
        mContext = aContext;
        mInflated = false;
   }

    private void init() {
        setBackgroundDrawable(new BitmapDrawable());
        setOutsideTouchable(true);
        setWindowLayoutMode(ViewGroup.LayoutParams.FILL_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        LayoutInflater inflater = LayoutInflater.from(mContext);
        RelativeLayout layout = (RelativeLayout) inflater.inflate(R.layout.doorhangerpopup, null);
        mContent = (LinearLayout) layout.findViewById(R.id.doorhanger_container);
        
        setContentView(layout);
        mInflated = true;
    }

    public void addDoorHanger(String message, String value, JSONArray buttons,
                              Tab tab, JSONObject options) {
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
                Log.i(LOGTAG, "JSON throws " + e);
            }
         }
        dh.setOptions(options);

        dh.setTab(tab);
        tab.addDoorHanger(value, dh);
        mContent.addView(dh);

        // Only update the popup if we're adding a notifcation to the selected tab
        if (tab.equals(Tabs.getInstance().getSelectedTab()))
            updatePopup();
    }

    // Updates popup contents to show doorhangers for the selected tab
    public void updatePopup() {
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

        showPopup();
    }

    public void hidePopup() {
        if (isShowing()) {
            Log.i(LOGTAG, "Hiding the DoorHangerPopup");
            dismiss();
        }
    }

    public void showPopup() {
        Log.i(LOGTAG, "Showing the DoorHangerPopup");
        fixBackgroundForFirst();

        if (isShowing())
            update();
        else
            showAsDropDown(GeckoApp.mBrowserToolbar.mFavicon);
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
