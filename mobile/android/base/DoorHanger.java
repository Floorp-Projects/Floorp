/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.widget.Divider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.text.style.URLSpan;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class DoorHanger extends LinearLayout {
    private static final String LOGTAG = "GeckoDoorHanger";

    private static int sInputPadding = -1;
    private static int sSpinnerTextColor = -1;
    private static int sSpinnerTextSize = -1;

    private static LayoutParams sButtonParams;
    static {
        sButtonParams = new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT, 1.0f);
    }

    private final TextView mTextView;
    private final LinearLayout mChoicesLayout;

    // Divider between doorhangers.
    private final View mDivider;

    // The tab associated with this notification.
    private final int mTabId;

    // Value used to identify the notification.
    private final String mValue;

    private List<PromptInput> mInputs;
    private CheckBox mCheckBox;

    private int mPersistence = 0;
    private boolean mPersistWhileVisible = false;
    private long mTimeout = 0;

    public interface OnButtonClickListener {
        public void onButtonClick(DoorHanger dh, String tag);
    }

    DoorHanger(Context context) {
        this(context, 0, null);
    }

    DoorHanger(Context context, int tabId, String value) {
        super(context);

        mTabId = tabId;
        mValue = value;

        if (sInputPadding == -1) {
            sInputPadding = getResources().getDimensionPixelSize(R.dimen.doorhanger_padding);
        }
        if (sSpinnerTextColor == -1) {
            sSpinnerTextColor = getResources().getColor(R.color.text_color_primary_disable_only);
        }
        if (sSpinnerTextSize == -1) {
            sSpinnerTextSize = getResources().getDimensionPixelSize(R.dimen.doorhanger_spinner_textsize);
        }

        setOrientation(VERTICAL);

        LayoutInflater.from(context).inflate(R.layout.doorhanger, this);
        mTextView = (TextView) findViewById(R.id.doorhanger_title);
        mChoicesLayout = (LinearLayout) findViewById(R.id.doorhanger_choices);
        mDivider = findViewById(R.id.divider_doorhanger);
    }

    int getTabId() {
        return mTabId;
    }

    String getValue() {
        return mValue;
    }

    List<PromptInput> getInputs() {
        return mInputs;
    }

    CheckBox getCheckBox() {
        return mCheckBox;
    }

    void showDivider() {
        mDivider.setVisibility(View.VISIBLE);
    }

    void hideDivider() {
        mDivider.setVisibility(View.GONE);
    }

    void setMessage(String message) {
        mTextView.setText(message);
    }

    void addLink(String label, String url, String delimiter) {
        String title = mTextView.getText().toString();
        SpannableString titleWithLink = new SpannableString(title + delimiter + label);
        URLSpan linkSpan = new URLSpan(url) {
            @Override
            public void onClick(View view) {
                Tabs.getInstance().loadUrlInTab(getURL());
            }
        };

        // Prevent text outside the link from flashing when clicked.
        ForegroundColorSpan colorSpan = new ForegroundColorSpan(mTextView.getCurrentTextColor());
        titleWithLink.setSpan(colorSpan, 0, title.length(), 0);

        titleWithLink.setSpan(linkSpan, title.length() + 1, titleWithLink.length(), 0);
        mTextView.setText(titleWithLink);
        mTextView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    void addButton(final String text, final String tag, final OnButtonClickListener listener) {
        final Button button = (Button) LayoutInflater.from(getContext()).inflate(R.layout.doorhanger_button, null);
        button.setText(text);
        button.setTag(tag);

        button.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onButtonClick(DoorHanger.this, tag);
            }
        });

        if (mChoicesLayout.getChildCount() == 0) {
            // If this is the first button we're adding, make the choices layout visible.
            mChoicesLayout.setVisibility(View.VISIBLE);
            findViewById(R.id.divider_choices).setVisibility(View.VISIBLE);
        } else {
            // Add a divider for additional buttons.
            Divider divider = new Divider(getContext(), null);
            divider.setOrientation(Divider.Orientation.VERTICAL);
            divider.setBackgroundColor(0xFFD1D5DA);
            mChoicesLayout.addView(divider);
        }

        mChoicesLayout.addView(button, sButtonParams);
    }

    void setOptions(final JSONObject options) {
        final int persistence = options.optInt("persistence");
        if (persistence > 0) {
            mPersistence = persistence;
        }

        mPersistWhileVisible = options.optBoolean("persistWhileVisible");

        final long timeout = options.optLong("timeout");
        if (timeout > 0) {
            mTimeout = timeout;
        }

        final JSONObject link = options.optJSONObject("link");
        if (link != null) {
            try {
                final String linkLabel = link.getString("label");
                final String linkUrl = link.getString("url");
                addLink(linkLabel, linkUrl, " ");
            } catch (JSONException e) { }
        }

        final JSONArray inputs = options.optJSONArray("inputs");
        if (inputs != null) {
            mInputs = new ArrayList<PromptInput>();

            final ViewGroup group = (ViewGroup) findViewById(R.id.doorhanger_inputs);
            group.setVisibility(VISIBLE);

            for (int i = 0; i < inputs.length(); i++) {
                try {
                    PromptInput input = PromptInput.getInput(inputs.getJSONObject(i));
                    mInputs.add(input);

                    View v = input.getView(getContext());
                    styleInput(input, v);
                    group.addView(v);
                } catch(JSONException ex) { }
            }
        }

        final String checkBoxText = options.optString("checkbox");
        if (!TextUtils.isEmpty(checkBoxText)) {
            mCheckBox = (CheckBox) findViewById(R.id.doorhanger_checkbox);
            mCheckBox.setText(checkBoxText);
            mCheckBox.setVisibility(VISIBLE);
        }
    }

    private void styleInput(PromptInput input, View view) {
        if (input instanceof PromptInput.MenulistInput) {
            styleSpinner(input, view);
        } else {
            // add some top and bottom padding to separate inputs
            view.setPadding(0, sInputPadding,
                            0, sInputPadding);
        }
    }

    private void styleSpinner(PromptInput input, View view) {
        PromptInput.MenulistInput spinInput = (PromptInput.MenulistInput) input;

        /* Spinners have some intrinsic padding. To force the spinner's text to line up with
         * the doorhanger text, we have to take that padding into account.
         * 
         * |-----A-------| <-- Normal doorhanger message
         * |-B-|---C+D---| <-- (optional) Spinner Label
         * |-B-|-C-|--D--| <-- Spinner
         *
         * A - Desired padding (sInputPadding)
         * B - Final padding applied to input element (sInputPadding - rect.left - textPadding).
         * C - Spinner background drawable padding (rect.left).
         * D - Spinner inner TextView padding (textPadding).
         */

        // First get the padding of the selected view inside the spinner. Since the spinner
        // hasn't been shown yet, we get this view directly from the adapter.
        Spinner spinner = spinInput.spinner;
        SpinnerAdapter adapter = spinner.getAdapter();
        View dropView = adapter.getView(0, null, spinner);
        int textPadding = 0;
        if (dropView != null) {
            textPadding = dropView.getPaddingLeft();
        }

        // Then get the intrinsic padding built into the background image of the spinner.
        Rect rect = new Rect();
        spinner.getBackground().getPadding(rect);

        // Set the difference in padding to the spinner view to align it with doorhanger text.
        view.setPadding(sInputPadding - rect.left - textPadding, 0, rect.right, sInputPadding);

        if (spinInput.textView != null) {
            spinInput.textView.setTextColor(sSpinnerTextColor);
            spinInput.textView.setTextSize(sSpinnerTextSize);

            // If this spinner has a label, offset it to also be aligned with the doorhanger text.
            spinInput.textView.setPadding(rect.left + textPadding, 0, 0, 0);
        }
    }


    /*
     * Checks with persistence and timeout options to see if it's okay to remove a doorhanger.
     *
     * @param isShowing Whether or not this doorhanger is currently visible to the user.
     *                 (e.g. the DoorHanger view might be VISIBLE, but its parent could be hidden)
     */
    boolean shouldRemove(boolean isShowing) {
        if (mPersistWhileVisible && isShowing) {
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
