/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.drawable.Drawable;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;

public class EraseViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
    public static final int LAYOUT_ID = R.layout.item_erase;

    private final SessionsSheetFragment fragment;

    public EraseViewHolder(final SessionsSheetFragment fragment, View itemView) {
        super(itemView);

        this.fragment = fragment;

        final TextView textView = (TextView) itemView;
        final Drawable leftDrawable = AppCompatResources.getDrawable(itemView.getContext(), R.drawable.ic_delete);
        textView.setCompoundDrawablesWithIntrinsicBounds(leftDrawable, null, null, null);
        textView.setOnClickListener(this);
    }

    @Override
    public void onClick(View view) {
        TelemetryWrapper.eraseInTabsTrayEvent();

        fragment.animateAndDismiss().addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                SessionManager.getInstance().removeAllSessions();
            }
        });
    }
}
