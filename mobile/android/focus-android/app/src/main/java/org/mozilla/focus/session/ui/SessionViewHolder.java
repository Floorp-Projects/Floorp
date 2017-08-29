/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.session.Session;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.utils.UrlUtils;

import java.lang.ref.WeakReference;

public class SessionViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
    /* package */ static final int LAYOUT_ID = R.layout.item_session;

    private final SessionsSheetFragment fragment;
    private final TextView textView;
    private WeakReference<Session> sessionReference;

    /* package */ SessionViewHolder(SessionsSheetFragment fragment, View itemView) {
        super(itemView);

        this.fragment = fragment;

        textView = (TextView) itemView;
        textView.setOnClickListener(this);
    }

    /* package */ void bind(Session session) {
        this.sessionReference = new WeakReference<>(session);

        updateUrl(session);

        final boolean isCurrentSession = SessionManager.getInstance().isCurrentSession(session);
        final int actionColor = ContextCompat.getColor(textView.getContext(), R.color.colorAction);

        textView.setTextColor(isCurrentSession ? actionColor : Color.BLACK);

        final Drawable drawable = AppCompatResources.getDrawable(itemView.getContext(), R.drawable.ic_link);
        if (drawable == null) {
            return;
        }

        final Drawable wrapDrawable = DrawableCompat.wrap(drawable);
        DrawableCompat.setTint(wrapDrawable, isCurrentSession ? actionColor : Color.BLACK);

        textView.setCompoundDrawablesWithIntrinsicBounds(wrapDrawable, null, null, null);
    }

    private void updateUrl(Session session) {
        final String shortUrl = UrlUtils.stripCommonSubdomains(
                UrlUtils.stripScheme(session.getUrl().getValue()));

        textView.setText(shortUrl);
    }

    @Override
    public void onClick(View view) {
        final Session sesison = sessionReference != null ? sessionReference.get() : null;
        if (sesison == null) {
            return;
        }

        fragment.animateAndDismiss().addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                final SessionManager sessionManager = SessionManager.getInstance();
                if (!sessionManager.isCurrentSession(sesison)) {
                    sessionManager.selectSession(sesison);
                }
            }
        });
    }
}
