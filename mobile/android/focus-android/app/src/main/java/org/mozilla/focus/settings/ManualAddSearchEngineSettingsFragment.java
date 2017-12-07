/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.design.widget.Snackbar;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;

import org.mozilla.focus.R;
import org.mozilla.focus.search.ManualAddSearchEnginePreference;
import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;

public class ManualAddSearchEngineSettingsFragment extends SettingsFragment {
    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
    }

    @Override
    public void onResume() {
        super.onResume();

        // We've checked that this cast is legal in super.onAttach.
        ((ActionBarUpdater) getActivity()).updateIcon(R.drawable.ic_close);
   }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.menu_search_engine_manual_add, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_save_search_engine:
                final View rootView = getView();
                final String engineName = ((EditText) rootView.findViewById(R.id.edit_engine_name)).getText().toString();
                final String searchQuery = ((EditText) rootView.findViewById(R.id.edit_search_string)).getText().toString();

                final ManualAddSearchEnginePreference pref = (ManualAddSearchEnginePreference) findPreference(getString(R.string.pref_key_manual_add_search_engine));
                final boolean engineValid = pref.validateEngineNameAndShowError(engineName);
                final boolean searchValid = pref.validateSearchQueryAndShowError(searchQuery);

                final boolean isSuccess = engineValid && searchValid;
                if (isSuccess) {
                    final SharedPreferences sharedPreferences = getSearchEngineSharedPreferences();
                    SearchEngineManager.addSearchEngine(sharedPreferences, getActivity(), engineName, searchQuery);
                    Snackbar.make(rootView, R.string.search_add_confirmation, Snackbar.LENGTH_SHORT).show();
                    getFragmentManager().popBackStack();
                }
                TelemetryWrapper.saveCustomSearchEngineEvent(isSuccess);
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }
}
