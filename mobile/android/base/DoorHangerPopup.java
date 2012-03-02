/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *   Sriram Ramasubramanian <sriram@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
