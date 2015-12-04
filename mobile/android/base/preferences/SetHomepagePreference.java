/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.preferences;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.DialogPreference;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;

public class SetHomepagePreference extends DialogPreference {
    private static final String DEFAULT_HOMEPAGE = AboutPages.HOME;

    private final SharedPreferences prefs;

    private RadioGroup homepageLayout;
    private RadioButton defaultRadio;
    private RadioButton userAddressRadio;
    private EditText homepageEditText;

    // This is the url that 1) was loaded from prefs or, 2) stored
    // when the user pressed the "default homepage" checkbox.
    private String storedUrl;

    public SetHomepagePreference(final Context context, final AttributeSet attrs) {
        super(context, attrs);
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

        homepageLayout = (RadioGroup) view.findViewById(R.id.homepage_layout);
        defaultRadio = (RadioButton) view.findViewById(R.id.radio_default);
        userAddressRadio = (RadioButton) view.findViewById(R.id.radio_user_address);
        homepageEditText = (EditText) view.findViewById(R.id.edittext_user_address);

        storedUrl = prefs.getString(GeckoPreferences.PREFS_HOMEPAGE, DEFAULT_HOMEPAGE);

        homepageLayout.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(final RadioGroup radioGroup, final int checkedId) {
                if (checkedId == R.id.radio_user_address) {
                    homepageEditText.setVisibility(View.VISIBLE);
                    openKeyboardAndSelectAll(getContext(), homepageEditText);
                } else {
                    homepageEditText.setVisibility(View.GONE);
                }
            }
        });
        setUIState(storedUrl);
    }

    private void setUIState(final String url) {
        if (isUrlDefaultHomepage(url)) {
            defaultRadio.setChecked(true);
        } else {
            userAddressRadio.setChecked(true);
            homepageEditText.setText(url);
        }
    }

    private boolean isUrlDefaultHomepage(final String url) {
        return TextUtils.isEmpty(url) || DEFAULT_HOMEPAGE.equals(url);
    }

    private static void openKeyboardAndSelectAll(final Context context, final View viewToFocus) {
        viewToFocus.requestFocus();
        viewToFocus.post(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.showSoftInput(viewToFocus, InputMethodManager.SHOW_IMPLICIT);
                // android:selectAllOnFocus doesn't work for the initial focus:
                // I'm not sure why. We manually selectAll instead.
                if (viewToFocus instanceof EditText) {
                    ((EditText) viewToFocus).selectAll();
                }
            }
        });
    }

    @Override
    protected void onDialogClosed(final boolean positiveResult) {
        super.onDialogClosed(positiveResult);
        if (positiveResult) {
            final SharedPreferences.Editor editor = prefs.edit();
            final String homePageEditTextValue = homepageEditText.getText().toString();
            final String newPrefValue;
            if (homepageLayout.getCheckedRadioButtonId() == R.id.radio_default ||
                    isUrlDefaultHomepage(homePageEditTextValue)) {
                newPrefValue = "";
                editor.remove(GeckoPreferences.PREFS_HOMEPAGE);
            } else {
                newPrefValue = homePageEditTextValue;
                editor.putString(GeckoPreferences.PREFS_HOMEPAGE, newPrefValue);
            }
            editor.apply();

            if (getOnPreferenceChangeListener() != null) {
                getOnPreferenceChangeListener().onPreferenceChange(this, newPrefValue);
            }
        }
    }
}
