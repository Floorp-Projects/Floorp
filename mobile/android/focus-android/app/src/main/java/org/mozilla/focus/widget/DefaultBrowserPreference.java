/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.os.Build;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.widget.Switch;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.Browsers;
import org.mozilla.focus.utils.SupportUtils;

public class DefaultBrowserPreference extends Preference {
    private Switch switchView;

    @SuppressWarnings("unused") // Instantiated from XML
    public DefaultBrowserPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    @SuppressWarnings("unused") // Instantiated from XML
    public DefaultBrowserPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setWidgetLayoutResource(R.layout.preference_default_browser);

        final String appName = getContext().getResources().getString(R.string.app_name);
        final String title = getContext().getResources().getString(R.string.preference_default_browser2, appName);

        setTitle(title);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        switchView = (Switch) holder.findViewById(R.id.switch_widget);

        update();
    }

    public void update() {
        if (switchView != null) {
            final Browsers browsers = new Browsers(getContext(), Browsers.TRADITIONAL_BROWSER_URL);
            switchView.setChecked(browsers.isDefaultBrowser(getContext()));
        }
    }

    @Override
    public void onClick() {
        final Context context = getContext();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            SupportUtils.INSTANCE.openDefaultAppsSettings(context);
        } else {
            SupportUtils.INSTANCE.openDefaultBrowserSumoPage(context);
        }
    }
}
