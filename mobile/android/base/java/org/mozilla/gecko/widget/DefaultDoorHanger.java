/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.support.v4.content.ContextCompat;
import android.text.Html;
import android.text.Spanned;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.prompts.PromptInput;
import org.mozilla.gecko.util.GeckoBundle;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

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
            sSpinnerTextColor = ContextCompat.getColor(context, R.color.text_color_primary_disable_only);
        }

        switch (mType) {
            case GEOLOCATION:
                mIcon.setImageResource(R.drawable.location);
                mIcon.setVisibility(VISIBLE);
                break;

            case DESKTOPNOTIFICATION2:
                mIcon.setImageResource(R.drawable.push_notification);
                mIcon.setVisibility(VISIBLE);
                break;
        }

        loadConfig(config);
    }

    @Override
    protected void loadConfig(DoorhangerConfig config) {
        final String message = config.getMessage();
        if (message != null) {
            setMessage(message);
        }

        final GeckoBundle options = config.getOptions();
        if (options != null) {
            setOptions(options);
        }

        final DoorhangerConfig.Link link = config.getLink();
        if (link != null) {
            addLink(link.label, link.url);
        }

        addButtonsToLayout(config);
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
    public void setOptions(final GeckoBundle options) {
        super.setOptions(options);

        final GeckoBundle[] inputs = options.getBundleArray("inputs");
        if (inputs != null) {
            mInputs = new ArrayList<PromptInput>();

            final ViewGroup group = (ViewGroup) findViewById(R.id.doorhanger_inputs);
            group.setVisibility(VISIBLE);

            for (int i = 0; i < inputs.length; i++) {
                PromptInput input = PromptInput.getInput(inputs[i]);
                mInputs.add(input);

                final int padding = mResources.getDimensionPixelSize(
                        R.dimen.doorhanger_section_padding_medium);
                final View v = input.getView(getContext());
                styleInput(input, v);
                v.setPadding(0, 0, 0, padding);
                group.addView(v);
            }
        }

        final String checkBoxText = options.getString("checkbox");
        if (!TextUtils.isEmpty(checkBoxText)) {
            mCheckBox = (CheckBox) findViewById(R.id.doorhanger_checkbox);
            mCheckBox.setText(checkBoxText);
            if (options.containsKey("checkboxState")) {
                final boolean checkBoxState = options.getBoolean("checkboxState");
                mCheckBox.setChecked(checkBoxState);
            }
            mCheckBox.setVisibility(VISIBLE);
        }
    }

    @Override
    protected OnClickListener makeOnButtonClickListener(final int id, final String telemetryExtra) {
        return new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                final String expandedExtra = mType.toString().toLowerCase(Locale.US) + "-" + telemetryExtra;
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.DOORHANGER, expandedExtra);

                final GeckoBundle response = new GeckoBundle(3);
                response.putInt("callback", id);

                final CheckBox checkBox = getCheckBox();
                // If the checkbox is being used, pass its value
                if (checkBox != null) {
                    response.putBoolean("checked", checkBox.isChecked());
                }

                final List<PromptInput> doorHangerInputs = getInputs();
                if (doorHangerInputs != null) {
                    final GeckoBundle inputs = new GeckoBundle();
                    for (final PromptInput input : doorHangerInputs) {
                        input.putInBundle(inputs);
                    }
                    response.putBundle("inputs", inputs);
                }

                mOnButtonClickListener.onButtonClick(response, DefaultDoorHanger.this);
            }
        };
    }

    private void setMessage(String message) {
        Spanned markupMessage = Html.fromHtml(message);
        mMessage.setText(markupMessage);
    }

    private void styleInput(PromptInput input, View view) {
        if (input instanceof PromptInput.MenulistInput) {
            styleDropdownInputs(input, view);
        }
        view.setPadding(0, 0, 0, mResources.getDimensionPixelSize(R.dimen.doorhanger_subsection_padding));
    }

    private void styleDropdownInputs(PromptInput input, View view) {
        PromptInput.MenulistInput spinInput = (PromptInput.MenulistInput) input;

        if (spinInput.textView != null) {
            spinInput.textView.setTextColor(sSpinnerTextColor);
        }
    }
}
