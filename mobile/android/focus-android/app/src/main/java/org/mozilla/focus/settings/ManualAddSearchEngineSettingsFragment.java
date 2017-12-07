/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.design.widget.Snackbar;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;

import org.mozilla.focus.R;
import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;

import java.util.Collections;

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

                final SharedPreferences sharedPreferences = getSearchEngineSharedPreferences();
                boolean isSuccess = false;
                if (TextUtils.isEmpty(engineName)) {
                    Snackbar.make(rootView, R.string.search_add_error_empty_name, Snackbar.LENGTH_SHORT).show();
                } else if (TextUtils.isEmpty(searchQuery)) {
                    Snackbar.make(rootView, R.string.search_add_error_empty_search, Snackbar.LENGTH_SHORT).show();
                } else if (!validateSearchFields(engineName, searchQuery, sharedPreferences)) {
                    Snackbar.make(rootView, R.string.search_add_error_format, Snackbar.LENGTH_SHORT).show();
                } else {
                    SearchEngineManager.addSearchEngine(sharedPreferences, getActivity(), engineName, searchQuery);
                    isSuccess = true;
                    Snackbar.make(rootView, R.string.search_add_confirmation, Snackbar.LENGTH_SHORT).show();
                    getFragmentManager().popBackStack();
                }
                TelemetryWrapper.saveCustomSearchEngineEvent(isSuccess);
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        ViewUtils.hideKeyboard(getActivity().getCurrentFocus());
    }

    private static boolean validateSearchFields(String engineName, String searchString, SharedPreferences sharedPreferences) {
        if (sharedPreferences.getStringSet(SearchEngineManager.PREF_KEY_CUSTOM_SEARCH_ENGINES,
                Collections.<String>emptySet()).contains(engineName)) {
            return false;
        }

        return UrlUtils.isValidSearchQueryUrl(searchString);
    }
}
