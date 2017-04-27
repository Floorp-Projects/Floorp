/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu;

import android.support.v7.widget.RecyclerView;
import android.view.View;

public abstract class BrowserMenuViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
    private BrowserMenu menu;
    private View.OnClickListener listener;

    public BrowserMenuViewHolder(View itemView) {
        super(itemView);
    }

    public void setMenu(BrowserMenu menu) {
        this.menu = menu;
    }

    public BrowserMenu getMenu() {
        return menu;
    }

    public void setOnClickListener(View.OnClickListener listener) {
        this.listener = listener;
    }

    @Override
    public void onClick(View view) {
        if (menu != null) {
            menu.dismiss();
        }

        if (listener != null) {
            listener.onClick(view);
        }
    }
}
