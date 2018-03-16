/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.preference.Preference;
import android.support.v4.content.ContextCompat;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.Switch;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.session.Source;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.SupportUtils;

/**
 * Ideally we'd extend SwitchPreference, and only do the summary modification. Unfortunately
 * that results in us using an older Switch which animates differently to the (seemingly AppCompat)
 * switches used in the remaining preferences. There's no AppCompat SwitchPreference to extend,
 * so instead we just build our own preference.
 */
class TelemetrySwitchPreference extends Preference {
    public TelemetrySwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public TelemetrySwitchPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init();
    }

    private void init() {
        setLayoutResource(R.layout.preference_telemetry);

        // We are keeping track of the preference value ourselves.
        setPersistent(false);
    }

    @Override
    protected void onBindView(final View view) {
        super.onBindView(view);

        final Switch switchWidget = view.findViewById(R.id.switch_widget);

        switchWidget.setChecked(TelemetryWrapper.isTelemetryEnabled(getContext()));

        switchWidget.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                TelemetryWrapper.setTelemetryEnabled(getContext(), isChecked);
            }
        });

        final Resources resources = view.getResources();

        final TextView summary = view.findViewById(android.R.id.summary);
        summary.setText(resources.getString(R.string.preference_mozilla_telemetry_summary2, resources.getString(R.string.app_name)));

        final TextView learnMoreLink = view.findViewById(R.id.link);
        learnMoreLink.setPaintFlags(learnMoreLink.getPaintFlags() | Paint.UNDERLINE_TEXT_FLAG);
        learnMoreLink.setTextColor(ContextCompat.getColor(view.getContext(), R.color.colorAction));
        learnMoreLink.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // This is a hardcoded link: if we ever end up needing more of these links, we should
                // move the link into an xml parameter, but there's no advantage to making it configurable now.
                final String url = SupportUtils.getSumoURLForTopic(getContext(), SupportUtils.SumoTopic.USAGE_DATA);
                SessionManager.getInstance().createSession(Source.MENU, url);
                ((Activity) getContext()).onBackPressed();
            }
        });

        final TypedArray backgroundDrawableArray = view.getContext().obtainStyledAttributes(new int[]{R.attr.selectableItemBackground});
        final Drawable backgroundDrawable = backgroundDrawableArray.getDrawable(0);
        backgroundDrawableArray.recycle();
        learnMoreLink.setBackground(backgroundDrawable);

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
