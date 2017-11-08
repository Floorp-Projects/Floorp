/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.Context;
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
import org.mozilla.focus.search.SearchEngine;
import org.mozilla.focus.utils.UrlUtils;

import java.util.Collections;

public class ManualAddSearchEngineSettingsFragment extends SettingsFragment {
    public static final int FRAGMENT_CLASS_TYPE = 1; // Unique SettingsFragment identifier

    public static SettingsFragment newInstance(Bundle intentArgs, int prefsResId, int titleResId) {
        SettingsFragment f = new ManualAddSearchEngineSettingsFragment();
        f.setArguments(makeArgumentBundle(intentArgs, prefsResId, titleResId));
        return f;
    }

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

                final SharedPreferences sharedPreferences = getActivity()
                        .getSharedPreferences(SearchEngine.PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE);
                if (!validateSearchFields(engineName, searchQuery, sharedPreferences)) {
                    Snackbar.make(rootView, R.string.search_add_error, Snackbar.LENGTH_SHORT).show();
                } else {
                    SearchEngine.addSearchEngine(sharedPreferences, getActivity(), engineName, searchQuery);
                    Snackbar.make(rootView, R.string.search_add_confirmation, Snackbar.LENGTH_SHORT).show();
                    getActivity().onBackPressed();
                }
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    private static boolean validateSearchFields(String engineName, String searchString, SharedPreferences sharedPreferences) {
        if (TextUtils.isEmpty(engineName)) {
            return false;
        }

        if (sharedPreferences.getStringSet(SearchEngine.PREF_KEY_CUSTOM_SEARCH_ENGINES,
                Collections.<String>emptySet()).contains(engineName)) {
            return false;
        }

        return UrlUtils.isValidSearchQueryUrl(searchString);
    }
}
