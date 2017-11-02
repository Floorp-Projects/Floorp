/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.mozilla.focus.R;

public class ManualAddSearchEngineSettingsFragment extends SettingsFragment {
    public static final int FRAGMENT_CLASS_TYPE = 1; // Unique SettingsFragment identifier

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // We've checked that this cast is legal in super.onAttach.
        ((ActionBarUpdater) getActivity()).updateIcon(R.drawable.ic_close);
        setHasOptionsMenu(true);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.menu_search_engine_manual_add, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_save_search_engine:
                // TODO: Save engineName and searchString
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }
}
