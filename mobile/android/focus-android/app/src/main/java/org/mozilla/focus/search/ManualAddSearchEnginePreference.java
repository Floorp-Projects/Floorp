/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.Preference;
import android.support.design.widget.TextInputLayout;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.UrlUtils;

public class ManualAddSearchEnginePreference extends Preference {
    private EditText engineNameEditText;
    private EditText searchQueryEditText;
    private TextInputLayout engineNameErrorLayout;
    private TextInputLayout searchQueryErrorLayout;

    public ManualAddSearchEnginePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ManualAddSearchEnginePreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected View onCreateView(ViewGroup parent) {
        final View view = super.onCreateView(parent);

        engineNameErrorLayout = view.findViewById(R.id.edit_engine_name_layout);
        searchQueryErrorLayout = view.findViewById(R.id.edit_search_string_layout);

        engineNameEditText = view.findViewById(R.id.edit_engine_name);
        engineNameEditText.addTextChangedListener(buildTextWatcherForErrorLayout(engineNameErrorLayout));
        searchQueryEditText = view.findViewById(R.id.edit_search_string);
        searchQueryEditText.addTextChangedListener(buildTextWatcherForErrorLayout(searchQueryErrorLayout));
        return view;
    }

    private boolean engineNameIsUnique(String engineName) {
        final SharedPreferences sharedPreferences = getContext().getSharedPreferences(SearchEngineManager.PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE);
        return !sharedPreferences.contains(engineName);
    }

    public boolean validateEngineNameAndShowError(String engineName) {
        if (TextUtils.isEmpty(engineName) || !engineNameIsUnique(engineName)) {
            engineNameErrorLayout.setError(getContext().getString(R.string.search_add_error_empty_name));
            return false;
        } else {
            engineNameErrorLayout.setError(null);
            return true;
        }
    }

    public boolean validateSearchQueryAndShowError(String searchQuery) {
        if (TextUtils.isEmpty(searchQuery)) {
            searchQueryErrorLayout.setError(getContext().getString(R.string.search_add_error_empty_search));
            return false;
        } else if (!UrlUtils.isValidSearchQueryUrl(searchQuery)) {
            searchQueryErrorLayout.setError(getContext().getString(R.string.search_add_error_format));
            return false;
        } else {
            searchQueryErrorLayout.setError(null);
            return true;
        }
    }

    private TextWatcher buildTextWatcherForErrorLayout(final TextInputLayout errorLayout) {
        return new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                errorLayout.setError(null);
            }

            @Override
            public void afterTextChanged(Editable editable) {}
        };
    }
}
