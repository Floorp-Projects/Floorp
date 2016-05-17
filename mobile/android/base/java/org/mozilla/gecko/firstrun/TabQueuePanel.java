/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.os.Bundle;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.SwitchCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.tabqueue.TabQueueHelper;
import org.mozilla.gecko.tabqueue.TabQueuePrompt;

public class TabQueuePanel extends FirstrunPanel {
    private static final int REQUEST_CODE_TAB_QUEUE = 1;
    private SwitchCompat toggleSwitch;
    private ImageView imageView;
    private TextView messageTextView;
    private TextView subtextTextView;
    private Context context;

    @Override
    public View onCreateView(LayoutInflater inflater, final ViewGroup container, Bundle savedInstance) {
        context = getContext();
        final View root = super.onCreateView(inflater, container, savedInstance);

        imageView = (ImageView) root.findViewById(R.id.firstrun_image);
        messageTextView = (TextView) root.findViewById(R.id.firstrun_text);
        subtextTextView = (TextView) root.findViewById(R.id.firstrun_subtext);

        toggleSwitch = (SwitchCompat) root.findViewById(R.id.firstrun_switch);
        toggleSwitch.setVisibility(View.VISIBLE);
        toggleSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.DIALOG, "firstrun_tabqueue-permissions");
                if (b && !TabQueueHelper.canDrawOverlays(context)) {
                    Intent promptIntent = new Intent(context, TabQueuePrompt.class);
                    startActivityForResult(promptIntent, REQUEST_CODE_TAB_QUEUE);
                    return;
                }

                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-tabqueue-" + b);

                final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
                final SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoPreferences.PREFS_TAB_QUEUE, b).apply();

                // Set image, text, and typeface changes.
                imageView.setImageResource(b ? R.drawable.firstrun_tabqueue_on : R.drawable.firstrun_tabqueue_off);
                messageTextView.setText(b ? R.string.firstrun_tabqueue_message_on : R.string.firstrun_tabqueue_message_off);
                messageTextView.setTypeface(b ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
                subtextTextView.setText(b ? R.string.firstrun_tabqueue_subtext_on : R.string.firstrun_tabqueue_subtext_off);
                subtextTextView.setTypeface(b ? Typeface.defaultFromStyle(Typeface.ITALIC) : Typeface.DEFAULT);
                subtextTextView.setTextColor(b ? ContextCompat.getColor(context, R.color.fennec_ui_orange) : ContextCompat.getColor(context, R.color.placeholder_grey));
            }
        });

        return root;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case REQUEST_CODE_TAB_QUEUE:
                final boolean accepted = TabQueueHelper.processTabQueuePromptResponse(resultCode, context);
                if (accepted) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.DIALOG, "firstrun_tabqueue-permissions-yes");
                    toggleSwitch.setChecked(true);
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "firstrun-tabqueue-true");
                }
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.DIALOG, "firstrun_tabqueue-permissions-" + (accepted ? "accepted" : "rejected"));
                break;
        }
    }

}
