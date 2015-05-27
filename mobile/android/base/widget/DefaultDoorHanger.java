/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.text.Html;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.text.style.URLSpan;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.prompts.PromptInput;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import org.mozilla.gecko.toolbar.SiteIdentityPopup;

import java.util.ArrayList;
import java.util.List;

public class DefaultDoorHanger extends DoorHanger {
    private static final String LOGTAG = "GeckoDefaultDoorHanger";

    private static int sSpinnerTextColor = -1;

    private final TextView mMessage;
    private List<PromptInput> mInputs;
    private CheckBox mCheckBox;

    public DefaultDoorHanger(Context context, DoorhangerConfig config, Type type) {
        super(context, config, type);

        mMessage = (TextView) findViewById(R.id.doorhanger_message);

        if (sSpinnerTextColor == -1) {
            sSpinnerTextColor = mResources.getColor(R.color.text_color_primary_disable_only);
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

        setButtons(config);
    }

    @Override
    protected int getContentResource() {
        return R.layout.default_doorhanger;
    }

    private List<PromptInput> getInputs() {
        return mInputs;
    }

    private CheckBox getCheckBox() {
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

    @Override
    protected OnClickListener makeOnButtonClickListener(final int id) {
        return new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                final JSONObject response = new JSONObject();
                try {
                    // TODO: Bug 1149359 - Split this into each Doorhanger Type class.
                    switch (mType) {
                        case MIXED_CONTENT:
                            response.put("allowContent", (id == SiteIdentityPopup.ButtonType.DISABLE.ordinal()));
                            response.put("contentType", ("mixed"));
                            break;
                        case TRACKING:
                            response.put("allowContent", (id == SiteIdentityPopup.ButtonType.DISABLE.ordinal()));
                            response.put("contentType", ("tracking"));
                            break;
                        default:
                            response.put("callback", id);

                            CheckBox checkBox = getCheckBox();
                            // If the checkbox is being used, pass its value
                            if (checkBox != null) {
                                response.put("checked", checkBox.isChecked());
                            }

                            List<PromptInput> doorHangerInputs = getInputs();
                            if (doorHangerInputs != null) {
                                JSONObject inputs = new JSONObject();
                                for (PromptInput input : doorHangerInputs) {
                                    inputs.put(input.getId(), input.getValue());
                                }
                                response.put("inputs", inputs);
                            }
                    }
                } catch (JSONException e) {
                    Log.e(LOGTAG, "Error creating onClick response", e);
                }

                mOnButtonClickListener.onButtonClick(response, DefaultDoorHanger.this);
            }
        };
    }

    private void setMessage(String message) {
        Spanned markupMessage = Html.fromHtml(message);
        mMessage.setText(markupMessage);
    }

    private void addLink(String label, String url, String delimiter) {
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
