/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Build;
import android.preference.Preference;
import android.provider.Browser;
import android.provider.Settings;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Switch;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.utils.Browsers;

@TargetApi(Build.VERSION_CODES.N)
public class DefaultBrowserPreference extends Preference {
    private Switch switchView;

    @SuppressWarnings("unused") // Instantiated from XML
    public DefaultBrowserPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setWidgetLayoutResource(R.layout.preference_default_browser);
    }

    @SuppressWarnings("unused") // Instantiated from XML
    public DefaultBrowserPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.preference_default_browser);
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        switchView = (Switch) view.findViewById(R.id.switch_widget);
        update();
    }

    public void update() {
        if (switchView != null) {
            final Browsers browsers = new Browsers(getContext(), "http://www.mozilla.org");
            switchView.setChecked(browsers.isDefaultBrowser(getContext()));
        }
    }

    @Override
    protected void onClick() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
            getContext().startActivity(intent);
        } else {
            final String url = getContext().getResources().getString(R.string.url_sumo_default_browser);

            final Intent intent = InfoActivity.getIntentFor(getContext(), url, getTitle().toString());
            getContext().startActivity(intent);
        }
    }
}
