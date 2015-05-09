/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.content.res.Resources;
import android.text.Html;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.text.style.URLSpan;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;

public abstract class DoorHanger extends LinearLayout {

    public static DoorHanger Get(Context context, DoorhangerConfig config) {
        final Type type = config.getType();
        switch (type) {
            case LOGIN:
                return new LoginDoorHanger(context, config);
            case TRACKING:
            case MIXED_CONTENT:
                return new DefaultDoorHanger(context, config, type);
        }
        return new DefaultDoorHanger(context, config, type);
    }

    public static enum Type { DEFAULT, LOGIN, TRACKING, MIXED_CONTENT}

    public interface OnButtonClickListener {
        public void onButtonClick(JSONObject response, DoorHanger doorhanger);
    }

    protected static final LayoutParams sButtonParams;
    static {
        sButtonParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1.0f);
    }

    private static final String LOGTAG = "GeckoDoorHanger";

    // Divider between doorhangers.
    private final View mDivider;

    protected final LinearLayout mButtonsContainer;
    protected final OnButtonClickListener mOnButtonClickListener;

    // The tab this doorhanger is associated with.
    private final int mTabId;

    // DoorHanger identifier.
    private final String mIdentifier;

    protected final Type mType;

    private final ImageView mIcon;
    private final TextView mMessage;

    protected final Context mContext;
    protected final Resources mResources;

    protected int mDividerColor;

    protected boolean mPersistWhileVisible;
    protected int mPersistenceCount;
    protected long mTimeout;

    protected DoorHanger(Context context, DoorhangerConfig config, Type type) {
        super(context);
        mContext = context;
        mResources = context.getResources();
        mTabId = config.getTabId();
        mIdentifier = config.getId();

        int resource;
        switch (type) {
            case LOGIN:
                resource = R.layout.login_doorhanger;
                break;
            default:
                resource = R.layout.doorhanger;
        }

        LayoutInflater.from(context).inflate(resource, this);
        mDivider = findViewById(R.id.divider_doorhanger);
        mIcon = (ImageView) findViewById(R.id.doorhanger_icon);
        mMessage = (TextView) findViewById(R.id.doorhanger_message);

        mType = type;

        mButtonsContainer = (LinearLayout) findViewById(R.id.doorhanger_buttons);
        mOnButtonClickListener = config.getButtonClickListener();

        mDividerColor = mResources.getColor(R.color.divider_light);
        setOrientation(VERTICAL);
    }

    protected abstract void loadConfig(DoorhangerConfig config);

    protected void setOptions(final JSONObject options) {
        final int persistence = options.optInt("persistence");
        if (persistence > 0) {
            mPersistenceCount = persistence;
        }

        mPersistWhileVisible = options.optBoolean("persistWhileVisible");

        final long timeout = options.optLong("timeout");
        if (timeout > 0) {
            mTimeout = timeout;
        }
    }

    protected void setButtons(DoorhangerConfig config) {
        final JSONArray buttons = config.getButtons();
        for (int i = 0; i < buttons.length(); i++) {
            try {
                final JSONObject buttonObject = buttons.getJSONObject(i);
                final String label = buttonObject.getString("label");
                final int callbackId = buttonObject.getInt("callback");
                addButtonToLayout(label, callbackId);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error creating doorhanger button", e);
            }
        }
   }

    public int getTabId() {
        return mTabId;
    }

    public String getIdentifier() {
        return mIdentifier;
    }

    public void showDivider() {
        mDivider.setVisibility(View.VISIBLE);
    }

    public void hideDivider() {
        mDivider.setVisibility(View.GONE);
    }

    public void setIcon(int resId) {
        mIcon.setImageResource(resId);
        mIcon.setVisibility(View.VISIBLE);
    }

    protected void setMessage(String message) {
        Spanned markupMessage = Html.fromHtml(message);
        mMessage.setText(markupMessage);
    }

    protected void addLink(String label, String url, String delimiter) {
        String title = mMessage.getText().toString();
        SpannableString titleWithLink = new SpannableString(title + delimiter + label);
        URLSpan linkSpan = new URLSpan(url) {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().loadUrlInTab(getURL());
            }
        };

        // Prevent text outside the link from flashing when clicked.
        ForegroundColorSpan colorSpan = new ForegroundColorSpan(mMessage.getCurrentTextColor());
        titleWithLink.setSpan(colorSpan, 0, title.length(), 0);

        titleWithLink.setSpan(linkSpan, title.length() + 1, titleWithLink.length(), 0);
        mMessage.setText(titleWithLink);
        mMessage.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Creates and adds a button into the DoorHanger.
     * @param text Button text
     * @param id Identifier associated with the button
     */
    private void addButtonToLayout(String text, int id) {
        final Button button = createButtonInstance(text, id);
        if (mButtonsContainer.getChildCount() == 0) {
            // If this is the first button we're adding, make the choices layout visible.
            mButtonsContainer.setVisibility(View.VISIBLE);
            // Make the divider above the buttons visible.
            View divider = findViewById(R.id.divider_buttons);
            divider.setVisibility(View.VISIBLE);
        } else {
            // Add a vertical divider between additional buttons.
            Divider divider = new Divider(getContext(), null);
            divider.setOrientation(Divider.Orientation.VERTICAL);
            divider.setBackgroundColor(mDividerColor);
            mButtonsContainer.addView(divider);
        }

        mButtonsContainer.addView(button, sButtonParams);
    }

    protected Button createButtonInstance(String text, int id) {
        final Button button = (Button) LayoutInflater.from(getContext()).inflate(R.layout.doorhanger_button, null);
        button.setText(text);
        button.setOnClickListener(makeOnButtonClickListener(id));
        return button;
    }

    protected abstract OnClickListener makeOnButtonClickListener(final int id);

    /*
     * Checks with persistence and timeout options to see if it's okay to remove a doorhanger.
     *
     * @param isShowing Whether or not this doorhanger is currently visible to the user.
     *                 (e.g. the DoorHanger view might be VISIBLE, but its parent could be hidden)
     */
    public boolean shouldRemove(boolean isShowing) {
        if (mPersistWhileVisible && isShowing) {
            // We still want to decrement mPersistence, even if the popup is showing
            if (mPersistenceCount != 0)
                mPersistenceCount--;
            return false;
        }

        // If persistence is set to -1, the doorhanger will never be
        // automatically removed.
        if (mPersistenceCount != 0) {
            mPersistenceCount--;
            return false;
        }

        if (System.currentTimeMillis() <= mTimeout) {
            return false;
        }

        return true;
    }
}
