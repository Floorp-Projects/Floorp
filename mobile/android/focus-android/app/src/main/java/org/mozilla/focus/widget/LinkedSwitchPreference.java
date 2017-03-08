/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.Switch;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.utils.SupportUtils;

/**
 * Ideally we'd extend SwitchPreference, and only do the summary modification. Unfortunately
 * that results in us using an older Switch which animates differently to the (seemingly AppCompat)
 * switches used in the remaining preferences. There's no AppCompat SwitchPreference to extend,
 * so instead we just build our own preference.
 */
class LinkedSwitchPreference extends Preference {

    public LinkedSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.preference_linkedswitch);
    }

    public LinkedSwitchPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        setWidgetLayoutResource(R.layout.preference_linkedswitch);
    }

    @Override
    protected void onBindView(final View view) {
        super.onBindView(view);

        final Switch switchWidget = (Switch) view.findViewById(R.id.switch_widget);

        switchWidget.setChecked(getPersistedBoolean(false));

        switchWidget.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                persistBoolean(isChecked);
            }
        });

        // The docs don't actually specify that R.id.summary will exist, but we rely on Android
        // using it in e.g. Fennec's AlignRightLinkPreference, so it should be safe to use it (especially
        // since we support a narrower set of Android versions in Focus).
        final TextView summary = (TextView) view.findViewById(android.R.id.summary);

        summary.setPaintFlags(summary.getPaintFlags() | Paint.UNDERLINE_TEXT_FLAG);
        summary.setTextColor(Color.WHITE);

        summary.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // This is a hardcoded link: if we ever end up needing more of these links, we should
                // move the link into an xml parameter, but there's no advantage to making it configurable now.
                final String url = SupportUtils.getSumoURLForTopic(getContext(), "usage-data");
                final String title = getTitle().toString();

                final Intent intent = InfoActivity.getIntentFor(getContext(), url, title);
                getContext().startActivity(intent);
            }
        });

        final TypedArray backgroundDrawableArray = view.getContext().obtainStyledAttributes(new int[]{R.attr.selectableItemBackground});
        final Drawable backgroundDrawable = backgroundDrawableArray.getDrawable(0);
        backgroundDrawableArray.recycle();
        summary.setBackground(backgroundDrawable);

        // We still want to allow toggling the pref by touching any part of the pref (except for
        // the "learn more" link)
        setOnPreferenceClickListener(new OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                switchWidget.toggle();
                return true;
            }
        });
    }
}
