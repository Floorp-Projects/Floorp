/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser;

import android.view.View;

import org.mozilla.focus.R;
import org.mozilla.focus.fragment.BrowserFragment;

public class NavigationItemViewHolder extends BrowserMenuViewHolder {
    public static final int LAYOUT_ID = R.layout.menu_navigation;

    final View refreshButton;
    final View stopButton;

    public NavigationItemViewHolder(View itemView, BrowserFragment fragment) {
        super(itemView);

        refreshButton = itemView.findViewById(R.id.refresh);
        refreshButton.setOnClickListener(this);

        stopButton = itemView.findViewById(R.id.stop);
        stopButton.setOnClickListener(this);

        updateLoading(fragment.getSession().getLoading());

        final View forwardView = itemView.findViewById(R.id.forward);
        if (!fragment.getSession().getCanGoForward()) {
            forwardView.setEnabled(false);
            forwardView.setAlpha(0.5f);
        } else {
            forwardView.setOnClickListener(this);
        }
    }

    public void updateLoading(boolean loading) {
        refreshButton.setVisibility(loading ? View.GONE : View.VISIBLE);
        stopButton.setVisibility(loading ? View.VISIBLE : View.GONE);
    }
}
