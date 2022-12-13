/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open;

import androidx.recyclerview.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.focus.R;

public class AppViewHolder extends RecyclerView.ViewHolder {
    public static final int LAYOUT_ID = R.layout.item_app;

    private final TextView titleView;
    private final ImageView iconView;

    /* package */ AppViewHolder(View itemView) {
        super(itemView);

        titleView = itemView.findViewById(R.id.title);
        iconView = itemView.findViewById(R.id.icon);
    }

    public void bind(final AppAdapter.App app, final AppAdapter.OnAppSelectedListener listener) {
        titleView.setText(app.getLabel());

        iconView.setImageDrawable(app.loadIcon());

        itemView.setOnClickListener(createListenerWrapper(app, listener));
    }

    private View.OnClickListener createListenerWrapper(final AppAdapter.App app, final AppAdapter.OnAppSelectedListener listener) {
        return v -> {
            if (listener != null) {
                listener.onAppSelected(app);
            }
        };
    }
}
