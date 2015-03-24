/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;
import org.mozilla.gecko.prompts.PromptInput;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;

import java.util.ArrayList;
import java.util.List;

public class DefaultDoorHanger extends DoorHanger {
    private static final String LOGTAG = "GeckoDefaultDoorHanger";

    private final Resources mResources;
    private static int sSpinnerTextColor = -1;

    private List<PromptInput> mInputs;
    private CheckBox mCheckBox;

    public DefaultDoorHanger(Context context, DoorhangerConfig config) {
        this(context, config, Type.DEFAULT);
    }

    public DefaultDoorHanger(Context context, DoorhangerConfig config, Type type) {
        super(context, config, type);

        mResources = getResources();

        if (sSpinnerTextColor == -1) {
            sSpinnerTextColor = getResources().getColor(R.color.text_color_primary_disable_only);
        }
        loadConfig(config);
    }

    @Override
    protected void loadConfig(DoorhangerConfig config) {
        final String message = config.getMessage();
        if (message != null) {
            setMessage(message);
        }

        final JSONObject options = config.getOptions();
        if (options != null) {
            setOptions(options);
        }

        final DoorhangerConfig.Link link = config.getLink();
        if (link != null) {
            addLink(link.label, link.url, link.delimiter);
        }
    }

    @Override
    public List<PromptInput> getInputs() {
        return mInputs;
    }

    @Override
    public CheckBox getCheckBox() {
        return mCheckBox;
    }

    @Override
    public void setOptions(final JSONObject options) {
        super.setOptions(options);
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

                    final int padding = mResources.getDimensionPixelSize(R.dimen.doorhanger_padding);
                    View v = input.getView(getContext());
                    styleInput(input, v);
                    v.setPadding(0, 0, 0, padding);
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
            styleDropdownInputs(input, view);
        }
        view.setPadding(0, 0, 0, mResources.getDimensionPixelSize(R.dimen.doorhanger_padding));
    }

    private void styleDropdownInputs(PromptInput input, View view) {
        PromptInput.MenulistInput spinInput = (PromptInput.MenulistInput) input;

        if (spinInput.textView != null) {
            spinInput.textView.setTextColor(sSpinnerTextColor);
        }
    }
}
