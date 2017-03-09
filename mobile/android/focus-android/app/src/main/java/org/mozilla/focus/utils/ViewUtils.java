/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;
import android.support.annotation.StringRes;
import android.support.design.widget.Snackbar;
import android.support.v4.content.ContextCompat;
import android.view.Gravity;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.TextView;

import org.mozilla.focus.R;

public class ViewUtils {
    public static void showKeyboard(View view) {
        InputMethodManager imm = (InputMethodManager) view.getContext()
                .getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(view, 0);
    }

    public static void hideKeyboard(View view) {
        InputMethodManager imm = (InputMethodManager) view.getContext()
                .getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    /**
     * Create a snackbar with Focus branding (See #193).
     */
    public static void showBrandedSnackbar(View view, @StringRes int resId) {
        final Context context = view.getContext();
        final Snackbar snackbar = Snackbar.make(view, resId, Snackbar.LENGTH_LONG);

        final View snackbarView = snackbar.getView();
        snackbarView.setBackgroundColor(ContextCompat.getColor(context, R.color.snackbarBackground));

        final TextView snackbarTextView = (TextView) snackbarView.findViewById(android.support.design.R.id.snackbar_text);
        snackbarTextView.setTextColor(ContextCompat.getColor(context, R.color.snackbarTextColor));
        snackbarTextView.setGravity(Gravity.CENTER);
        snackbarTextView.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);

        snackbar.show();
    }
}
