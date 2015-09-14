/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.preferences;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.DialogPreference;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;

public class SetHomepagePreference extends DialogPreference {
    SharedPreferences prefs;
    EditText homepageTextEdit;
    CheckBox useCurrentTabCheck;

    public SetHomepagePreference(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        setPersistent(false);
        setDialogLayoutResource(R.layout.preference_set_homepage);

        prefs = GeckoSharedPrefs.forProfile(context);
    }

    @Override
    protected void onPrepareDialogBuilder(AlertDialog.Builder builder) {
        // Without this GB devices have a black background to the dialog.
        builder.setInverseBackgroundForced(true);
    }

    @Override
    protected void onBindDialogView(final View view) {
        super.onBindDialogView(view);

        homepageTextEdit = (EditText) view.findViewById(R.id.homepage_url);
        homepageTextEdit.requestFocus();

        homepageTextEdit.post(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.showSoftInput(homepageTextEdit, InputMethodManager.SHOW_IMPLICIT);
                homepageTextEdit.selectAll();
            }
        });

        final String url = prefs.getString(GeckoPreferences.PREFS_HOMEPAGE, AboutPages.HOME);
        if (!AboutPages.HOME.equals(url)) {
            homepageTextEdit.setText(url);
        }

        useCurrentTabCheck = (CheckBox) view.findViewById(R.id.use_current_checkbox);

        view.findViewById(R.id.use_current_title).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(final View v) {
                useCurrentTabCheck.toggle();
            }
        });

        useCurrentTabCheck.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(final CompoundButton buttonView, final boolean isChecked) {
                homepageTextEdit.setEnabled(!isChecked);
                if (isChecked) {
                    homepageTextEdit.setText(Tabs.getInstance().getSelectedTab().getURL());
                } else {
                    homepageTextEdit.selectAll();
                }
            }
        });
    }

    @Override
    protected void onDialogClosed(final boolean positiveResult) {
        super.onDialogClosed(positiveResult);
        if (positiveResult) {
            final SharedPreferences.Editor editor = prefs.edit();
            String newValue = homepageTextEdit.getText().toString();
            if (AboutPages.HOME.equals(newValue) || TextUtils.isEmpty(newValue)) {
                editor.remove(GeckoPreferences.PREFS_HOMEPAGE);
                newValue = "";
            } else {
                editor.putString(GeckoPreferences.PREFS_HOMEPAGE, newValue);
            }
            editor.apply();
            if (getOnPreferenceChangeListener() != null) {
                getOnPreferenceChangeListener().onPreferenceChange(this, newValue);
            }
        }
    }
}
