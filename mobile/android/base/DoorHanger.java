/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.widget.Divider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.text.style.URLSpan;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;

public class DoorHanger extends LinearLayout implements Button.OnClickListener {
    private static final String LOGTAG = "GeckoDoorHanger";

    private GeckoApp mActivity;
    // The popup that holds this doorhanger
    private DoorHangerPopup mPopup;
    private LinearLayout mChoicesLayout;
    private TextView mTextView;

    // LayoutParams used for adding button layouts
    static private LayoutParams mLayoutParams;
    private final int mTabId;
    // Value used to identify the notification
    private final String mValue;

    // Optional checkbox added underneath message text
    private CheckBox mCheckBox;

    // Divider between doorhangers.
    private View mDivider;

    private int mPersistence = 0;
    private boolean mPersistWhileVisible = false;
    private long mTimeout = 0;

    DoorHanger(GeckoApp aActivity, DoorHangerPopup aPopup, int aTabId, String aValue) {
        super(aActivity);

        mActivity = aActivity;
        mPopup = aPopup;
        mTabId = aTabId;
        mValue = aValue;
    }
 
    int getTabId() {
        return mTabId;
    }

    String getValue() {
        return mValue;
    }

    public void showDivider() {
        mDivider.setVisibility(View.VISIBLE);
    }

    public void hideDivider() {
        mDivider.setVisibility(View.GONE);
    }

    // Postpone stuff that needs to be done on the main thread
    void init(String message, JSONArray buttons, JSONObject options) {
        setOrientation(VERTICAL);

        LayoutInflater.from(mActivity).inflate(R.layout.doorhanger, this);
        setVisibility(View.GONE);

        mTextView = (TextView) findViewById(R.id.doorhanger_title);
        mTextView.setText(message);

        mChoicesLayout = (LinearLayout) findViewById(R.id.doorhanger_choices);

        mDivider = findViewById(R.id.divider_doorhanger);

        // Set the doorhanger text and buttons
        for (int i = 0; i < buttons.length(); i++) {
            try {
                JSONObject buttonObject = buttons.getJSONObject(i);
                String label = buttonObject.getString("label");
                int callBackId = buttonObject.getInt("callback");
                addButton(label, callBackId);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error creating doorhanger button", e);
            }
         }

        // Enable the button layout if we have buttons.
        if (buttons.length() > 0) {
            findViewById(R.id.divider_choices).setVisibility(View.VISIBLE);
            mChoicesLayout.setVisibility(View.VISIBLE);
        }

        setOptions(options);
    }

    private void addButton(String aText, int aCallback) {
        if (mLayoutParams == null)
            mLayoutParams = new LayoutParams(LayoutParams.FILL_PARENT,
                                             LayoutParams.FILL_PARENT,
                                             1.0f);

        Button button = (Button) LayoutInflater.from(mActivity).inflate(R.layout.doorhanger_button, null);
        button.setText(aText);
        button.setTag(Integer.toString(aCallback));
        button.setOnClickListener(this);

        if (mChoicesLayout.getChildCount() > 0) {
            Divider divider = new Divider(mActivity, null);
            divider.setOrientation(Divider.Orientation.VERTICAL);
            divider.setBackgroundColor(0xFFD1D5DA);
            mChoicesLayout.addView(divider);
        }

        mChoicesLayout.addView(button, mLayoutParams);
    }

    @Override
    public void onClick(View v) {
        JSONObject response = new JSONObject();
        try {
            response.put("callback", v.getTag().toString());

            // If the checkbox is being used, pass its value
            if (mCheckBox != null)
                response.put("checked", mCheckBox.isChecked());
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error creating onClick response", e);
        }

        GeckoEvent e = GeckoEvent.createBroadcastEvent("Doorhanger:Reply", response.toString());
        GeckoAppShell.sendEventToGecko(e);
        mPopup.removeDoorHanger(this);

        // This will hide the doorhanger (and hide the popup if there are no
        // more doorhangers to show)
        mPopup.updatePopup();
    }

    private void setOptions(JSONObject options) {
        try {
            mPersistence = options.getInt("persistence");
        } catch (JSONException e) { }

        try {
            mPersistWhileVisible = options.getBoolean("persistWhileVisible");
        } catch (JSONException e) { }

        try {
            mTimeout = options.getLong("timeout");
        } catch (JSONException e) { }

        try {
            JSONObject link = options.getJSONObject("link");
            String title = mTextView.getText().toString();
            String linkLabel = link.getString("label");
            String linkUrl = link.getString("url");
            SpannableString titleWithLink = new SpannableString(title + " " + linkLabel);
            URLSpan linkSpan = new URLSpan(linkUrl) {
                @Override
                public void onClick(View view) {
                    Tabs.getInstance().loadUrlInTab(this.getURL());
                }
            };

            // prevent text outside the link from flashing when clicked
            ForegroundColorSpan colorSpan = new ForegroundColorSpan(mTextView.getCurrentTextColor());
            titleWithLink.setSpan(colorSpan, 0, title.length(), 0);

            titleWithLink.setSpan(linkSpan, title.length() + 1, titleWithLink.length(), 0);
            mTextView.setText(titleWithLink);
            mTextView.setMovementMethod(LinkMovementMethod.getInstance());
        } catch (JSONException e) { }

        try {
            String checkBoxText = options.getString("checkbox");
            mCheckBox = (CheckBox) findViewById(R.id.doorhanger_checkbox);
            mCheckBox.setText(checkBoxText);
            mCheckBox.setVisibility(VISIBLE);
        } catch (JSONException e) { }
    }

    // This method checks with persistence and timeout options to see if
    // it's okay to remove a doorhanger.
    boolean shouldRemove() {
        if (mPersistWhileVisible && mPopup.isShowing()) {
            // We still want to decrement mPersistence, even if the popup is showing
            if (mPersistence != 0)
                mPersistence--;
            return false;
        }

        // If persistence is set to -1, the doorhanger will never be
        // automatically removed.
        if (mPersistence != 0) {
            mPersistence--;
            return false;
        }

        if (System.currentTimeMillis() <= mTimeout) {
            return false;
        }

        return true;
    }
}
