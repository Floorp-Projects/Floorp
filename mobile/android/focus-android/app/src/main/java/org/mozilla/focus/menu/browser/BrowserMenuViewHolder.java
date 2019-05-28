/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser;

import androidx.recyclerview.widget.RecyclerView;
import android.view.View;

import org.mozilla.focus.fragment.BrowserFragment;

public abstract class BrowserMenuViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
    private BrowserMenu menu;
    protected BrowserFragment browserFragment;

    public BrowserMenuViewHolder(View itemView) {
        super(itemView);
    }

    public void setMenu(BrowserMenu menu) {
        this.menu = menu;
    }

    public BrowserMenu getMenu() {
        return menu;
    }

    public void setOnClickListener(BrowserFragment browserFragment) {
        this.browserFragment = browserFragment;
    }

    @Override
    public void onClick(View view) {
        if (menu != null) {
            menu.dismiss();
        }

        if (browserFragment != null) {
            browserFragment.onClick(view);
        }
    }
}
