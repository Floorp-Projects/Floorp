/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.customtabs.CustomTabConfig;
import org.mozilla.focus.fragment.BrowserFragment;
import org.mozilla.focus.utils.ViewUtils;

/**
 * The overflow menu shown in the BrowserFragment containing page actions like "Refresh", "Share" etc.
 */
public class BrowserMenu extends PopupWindow {
    private BrowserMenuAdapter adapter;

    public BrowserMenu(Context context, BrowserFragment fragment, final @Nullable CustomTabConfig customTabConfig) {
        @SuppressLint("InflateParams") // This View will have it's params ignored anyway:
        final View view = LayoutInflater.from(context).inflate(R.layout.menu, null);
        setContentView(view);

        adapter = new BrowserMenuAdapter(context, this, fragment, customTabConfig);

        final RecyclerView menuList = view.findViewById(R.id.list);
        menuList.setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
        menuList.setAdapter(adapter);

        if (customTabConfig != null) {
            final TextView brandingView = view.findViewById(R.id.branding);
            brandingView.setText(context.getString(R.string.menu_custom_tab_branding, context.getString(R.string.app_name)));
            brandingView.setVisibility(View.VISIBLE);
        }

        setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        setFocusable(true);

        setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);

        setElevation(context.getResources().getDimension(R.dimen.menu_elevation));
    }

    public void updateTrackers(int trackers) {
        adapter.updateTrackers(trackers);
    }

    public void updateLoading(boolean loading) {
        adapter.updateLoading(loading);
    }

    public void show(View anchor) {
        final int xOffset = ViewUtils.isRTL(anchor) ? -anchor.getWidth() : 0;

        super.showAsDropDown(anchor, xOffset, -(anchor.getHeight() + anchor.getPaddingBottom()));
    }
}
