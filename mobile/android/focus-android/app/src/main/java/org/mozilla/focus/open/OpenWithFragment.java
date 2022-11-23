/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatDialogFragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.bottomsheet.BottomSheetBehavior;
import com.google.android.material.bottomsheet.BottomSheetDialog;

import org.mozilla.focus.GleanMetrics.OpenWith;
import org.mozilla.focus.R;
import org.mozilla.focus.ext.ContextKt;
import org.mozilla.focus.telemetry.TelemetryWrapper;

public class OpenWithFragment extends AppCompatDialogFragment implements AppAdapter.OnAppSelectedListener {
    public static final String FRAGMENT_TAG = "open_with";

    private static final String ARGUMENT_KEY_APPS = "apps";
    private static final String ARGUMENT_URL = "url";
    private static final String ARGUMENT_STORE = "store";

    public static OpenWithFragment newInstance(ActivityInfo[] apps, String url, ActivityInfo store) {
        final Bundle arguments = new Bundle();
        arguments.putParcelableArray(ARGUMENT_KEY_APPS, apps);
        arguments.putString(ARGUMENT_URL, url);
        arguments.putParcelable(ARGUMENT_STORE, store);

        final OpenWithFragment fragment = new OpenWithFragment();
        fragment.setArguments(arguments);

        return fragment;
    }

    @Override
    public void onPause() {
        getActivity().getSupportFragmentManager()
                .beginTransaction()
                .remove(this)
                .commitAllowingStateLoss();

        super.onPause();
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        ContextThemeWrapper wrapper = new ContextThemeWrapper(getContext(), android.R.style.Theme_Material_Light);

        @SuppressLint("InflateParams") // This View will have it's params ignored anyway:
        final View view = LayoutInflater.from(wrapper).inflate(R.layout.fragment_open_with, null);

        final Dialog dialog = new CustomWidthBottomSheetDialog(wrapper);
        dialog.setContentView(view);

        final RecyclerView appList = view.findViewById(R.id.apps);
        appList.setLayoutManager(new LinearLayoutManager(wrapper, RecyclerView.VERTICAL, false));

        AppAdapter adapter = new AppAdapter(
                wrapper,
                (ActivityInfo[]) getArguments().getParcelableArray(ARGUMENT_KEY_APPS),
                getArguments().getParcelable(ARGUMENT_STORE));
        adapter.setOnAppSelectedListener(this);
        appList.setAdapter(adapter);

        return dialog;
    }

    static class CustomWidthBottomSheetDialog extends BottomSheetDialog {
        private View contentView;

        private CustomWidthBottomSheetDialog(@NonNull Context context) {
            super(context);
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // The support library makes the bottomsheet full width on all devices (and then uses a 16:9
            // keyline). On tablets, the system bottom sheets use a narrower width - lets do that too:
            if (ContextKt.isTablet(getContext())) {
                int width = getContext().getResources().getDimensionPixelSize(R.dimen.tablet_bottom_sheet_width);
                final Window window = getWindow();
                if (window != null) {
                    window.setLayout(width, ViewGroup.LayoutParams.MATCH_PARENT);
                }
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
            if (ContextKt.isTablet(getContext())) {
                final int peekHeight = getContext().getResources().getDimensionPixelSize(R.dimen.tablet_bottom_sheet_peekheight);

                BottomSheetBehavior<View> bsBehaviour = BottomSheetBehavior.from((View) contentView.getParent());
                bsBehaviour.setPeekHeight(peekHeight);
            }

            super.show();
        }
    }

    @Override
    public void onAppSelected(AppAdapter.App app) {
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(getArguments().getString(ARGUMENT_URL)));
        intent.setClassName(app.getPackageName(), app.getActivityName());
        startActivity(intent);

        OpenWith.INSTANCE.listItemTapped().record(new OpenWith.ListItemTappedExtra(app.getPackageName().contains("mozilla")));

        if (app.getPackageName().contains("firefox")) {
            TelemetryWrapper.openFirefoxEvent();
        }

        dismiss();
    }
}
