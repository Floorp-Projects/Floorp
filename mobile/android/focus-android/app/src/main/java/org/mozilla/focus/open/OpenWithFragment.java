/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open;

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.LayoutRes;
import android.support.annotation.NonNull;
import android.support.design.widget.BottomSheetBehavior;
import android.support.design.widget.BottomSheetDialog;
import android.support.v7.app.AppCompatDialogFragment;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;

public class OpenWithFragment extends AppCompatDialogFragment implements AppAdapter.OnAppSelectedListener {
    public static final String FRAGMENT_TAG = "open_with";

    private static final String ARGUMENT_KEY_APPS = "apps";
    private static final String ARGUMENT_URL = "url";

    public static OpenWithFragment newInstance(ActivityInfo[] apps, String url) {
        final Bundle arguments = new Bundle();
        arguments.putParcelableArray(ARGUMENT_KEY_APPS, apps);
        arguments.putString(ARGUMENT_URL, url);

        final OpenWithFragment fragment = new OpenWithFragment();
        fragment.setArguments(arguments);

        return fragment;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        ContextThemeWrapper wrapper = new ContextThemeWrapper(getContext(), android.R.style.Theme_Material_Light);

        final View view = LayoutInflater.from(wrapper).inflate(R.layout.fragment_open_with, null);

        final Dialog dialog = new CustomWidthBottomSheetDialog(wrapper);
        dialog.setContentView(view);

        final RecyclerView appList = (RecyclerView) view.findViewById(R.id.apps);
        appList.setLayoutManager(new LinearLayoutManager(wrapper, LinearLayoutManager.VERTICAL, false));

        AppAdapter adapter = new AppAdapter(wrapper, (ActivityInfo[]) getArguments().getParcelableArray(ARGUMENT_KEY_APPS));
        adapter.setOnAppSelectedListener(this);
        appList.setAdapter(adapter);

        return dialog;
    }

    static class CustomWidthBottomSheetDialog extends BottomSheetDialog {
        private View contentView;

        public CustomWidthBottomSheetDialog(@NonNull Context context) {
            super(context);
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // The support library makes the bottomsheet full width on all devices (and then uses a 16:9
            // keyline). On tablets, the system bottom sheets use a narrower width - lets do that too:
            if (getContext().getResources().getBoolean(R.bool.is_tablet)) {
                int width = getContext().getResources().getDimensionPixelSize(R.dimen.bottom_sheet_width);
                getWindow().setLayout(width, ViewGroup.LayoutParams.MATCH_PARENT);
            }
        }

        @Override
        public void setContentView(View contentView) {
            super.setContentView(contentView);

            this.contentView = contentView;
        }

        @Override
        public void setContentView(@LayoutRes int layoutResId) {
            throw new IllegalStateException("CustomWidthBottomSheetDialog only supports setContentView(View)");
        }

        @Override
        public void setContentView(View view, ViewGroup.LayoutParams params) {
            throw new IllegalStateException("CustomWidthBottomSheetDialog only supports setContentView(View)");
        }

        @Override
        public void show() {
            if (getContext().getResources().getBoolean(R.bool.is_tablet)) {
                final int peekHeight = getContext().getResources().getDimensionPixelSize(R.dimen.bottom_sheet_peekheight);

                BottomSheetBehavior<View> bsBehaviour = BottomSheetBehavior.from((View) contentView.getParent());
                bsBehaviour.setPeekHeight(peekHeight);
            }

            super.show();
        }
    }

    @Override
    public void onAppSelected(AppAdapter.App app) {
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(getArguments().getString(ARGUMENT_URL)));
        intent.setPackage(app.getPackageName());
        startActivity(intent);

        dismiss();
    }
}
