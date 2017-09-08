/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.app.Dialog;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.mozilla.focus.R;
import org.mozilla.focus.shortcut.HomeScreen;
import org.mozilla.focus.shortcut.IconGenerator;
import org.mozilla.focus.telemetry.TelemetryWrapper;

/**
 * Fragment displaying a dialog where a user can change the title for a homescreen shortcut
 */
public class AddToHomescreenDialogFragment extends DialogFragment {
    public static final String FRAGMENT_TAG = "add-to-homescreen-prompt-dialog";
    private static final String URL = "url";
    private static final String TITLE = "title";
    private static final String BLOCKING_ENABLED = "blocking_enabled";

    public static AddToHomescreenDialogFragment newInstance(final String url, final String title, final boolean blockingEnabled) {
        AddToHomescreenDialogFragment frag = new AddToHomescreenDialogFragment();
        final Bundle args = new Bundle();
        args.putString(URL, url);
        args.putString(TITLE, title);
        args.putBoolean(BLOCKING_ENABLED, blockingEnabled);
        frag.setArguments(args);
        return frag;
    }

    @NonNull
    @Override
    public AlertDialog onCreateDialog(Bundle bundle) {
        final String url = getArguments().getString(URL);
        final String title = getArguments().getString(TITLE);
        final boolean blockingEnabled = getArguments().getBoolean(BLOCKING_ENABLED);

        final AlertDialog.Builder builder = new AlertDialog.Builder(getActivity(), R.style.DialogStyle);
        builder.setCancelable(true);
        builder.setTitle(getActivity().getString(R.string.menu_add_to_home_screen));

        final LayoutInflater inflater = getActivity().getLayoutInflater();
        final View dialogView = inflater.inflate(R.layout.add_to_homescreen, null);
        builder.setView(dialogView);

        final Bitmap icon = IconGenerator.generateLauncherIcon(getActivity(), url);
        final Drawable d = new BitmapDrawable(getResources(), icon);
        final ImageView iconView = (ImageView) dialogView.findViewById(R.id.homescreen_icon);
        iconView.setImageDrawable(d);

        final ImageView blockIcon = (ImageView) dialogView.findViewById(R.id.homescreen_dialog_block_icon);
        blockIcon.setImageResource(R.drawable.ic_tracking_protection_16_disabled);

        final Button addToHomescreenDialogCancelButton = (Button) dialogView.findViewById(R.id.addtohomescreen_dialog_cancel);
        final Button addToHomescreenDialogConfirmButton = (Button) dialogView.findViewById(R.id.addtohomescreen_dialog_add);
        addToHomescreenDialogCancelButton.setText(getString(R.string.dialog_addtohomescreen_action_cancel));
        addToHomescreenDialogConfirmButton.setText(getString(R.string.dialog_addtohomescreen_action_add));

        final LinearLayout warning = (LinearLayout) dialogView.findViewById(R.id.homescreen_dialog_warning_layout);
        warning.setVisibility(blockingEnabled ? View.GONE : View.VISIBLE);

        final EditText editableTitle = (EditText) dialogView.findViewById(R.id.edit_title);

        if (!TextUtils.isEmpty(title)) {
            editableTitle.setText(title);
            editableTitle.setSelection(title.length());
        }

        addToHomescreenDialogCancelButton.setOnClickListener( new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                TelemetryWrapper.cancelAddToHomescreenShortcutEvent();
                dismiss();
            }});

        addToHomescreenDialogConfirmButton.setOnClickListener( new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                HomeScreen.installShortCut(getActivity(), icon, url, editableTitle.getText().toString().trim(), blockingEnabled);
                TelemetryWrapper.addToHomescreenShortcutEvent();
                dismiss();
            }});

        return builder.create();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        final Dialog dialog = getDialog();
        if (dialog != null) {
            final Window window = dialog.getWindow();
            if (window != null) {
                window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
            }
        }
    }
}
