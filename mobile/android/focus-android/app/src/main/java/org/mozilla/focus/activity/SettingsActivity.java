/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.support.annotation.Nullable;
import android.support.v7.app.ActionBar;
import android.support.v7.widget.Toolbar;
import android.view.View;

import org.mozilla.focus.R;
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity;
import org.mozilla.focus.settings.ManualAddSearchEngineSettingsFragment;
import org.mozilla.focus.settings.SettingsFragment;

public class SettingsActivity extends LocaleAwareAppCompatActivity implements SettingsFragment.ActionBarUpdater {
    public static final int ACTIVITY_RESULT_LOCALE_CHANGED = 1;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_settings);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        final ActionBar actionBar = getSupportActionBar();
        assert actionBar != null;

        actionBar.setDisplayHomeAsUpEnabled(true);

        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                finish();
            }
        });

        final Bundle extras = getIntent().getExtras();
        final PreferenceFragment fragment;
        if (extras != null && extras.containsKey(SettingsFragment.FRAGMENT_CLASS_INTENT_EXTRA)) {
            switch (extras.getInt(SettingsFragment.FRAGMENT_CLASS_INTENT_EXTRA)) {
                case ManualAddSearchEngineSettingsFragment.FRAGMENT_CLASS_TYPE:
                    fragment = new ManualAddSearchEngineSettingsFragment();
                    break;
                default:
                    fragment = new SettingsFragment();
            }
        } else {
            fragment = new SettingsFragment();
        }
        fragment.setArguments(getIntent().getExtras());

        getFragmentManager().beginTransaction()
                .replace(R.id.container, fragment)
                .commit();

        // Ensure all locale specific Strings are initialised on first run, we don't set the title
        // anywhere before now (the title can only be set via AndroidManifest, and ensuring
        // that that loads the correct locale string is tricky).
        applyLocale();
    }

    @Override
    public void applyLocale() {
        setTitle(R.string.menu_settings);
    }

    @Override
    public void updateTitle(int titleResId) {
        setTitle(titleResId);
    }

    @Override
    public void updateIcon(int iconResId) {
        getSupportActionBar().setHomeAsUpIndicator(iconResId);
    }
}
