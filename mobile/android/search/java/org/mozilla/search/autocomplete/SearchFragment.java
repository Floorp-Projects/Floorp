/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;


import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import org.mozilla.search.R;

import java.util.List;

/**
 * A fragment to handle autocomplete. Its interface with the outside
 * world should be very very limited.
 * <p/>
 * TODO: Add more search providers (other than the dictionary)
 */
public class SearchFragment extends Fragment
        implements TextView.OnEditorActionListener, AcceptsJumpTaps {

    private static final int LOADER_ID_SUGGESTION = 0;
    private static final String KEY_SEARCH_TERM = "search_term";

    // Timeout for the suggestion client to respond
    private static final int SUGGESTION_TIMEOUT = 3000;

    // Maximum number of results returned by the suggestion client
    private static final int SUGGESTION_MAX = 5;

    private SuggestClient suggestClient;
    private SuggestionLoaderCallbacks suggestionLoaderCallbacks;

    private InputMethodManager inputMethodManager;
    private AutoCompleteAdapter autoCompleteAdapter;

    private View mainView;
    private View searchBar;
    private EditText editText;
    private Button clearButton;
    private ListView suggestionDropdown;

    private State state = State.WAITING;

    private static enum State {
        WAITING,  // The user is doing something else in the app.
        RUNNING   // The user is in search mode.
    }

    public SearchFragment() {
        // Required empty public constructor
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        // TODO: Don't hard-code this template string (bug 1039758)
        final String template = "https://search.yahoo.com/sugg/ff?" +
                "output=fxjson&appid=ffm&command=__searchTerms__&nresults=" + SUGGESTION_MAX;

        suggestClient = new SuggestClient(activity, template, SUGGESTION_TIMEOUT, SUGGESTION_MAX);
        suggestionLoaderCallbacks = new SuggestionLoaderCallbacks();

        inputMethodManager = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        autoCompleteAdapter = new AutoCompleteAdapter(activity, this);
    }

    @Override
    public void onDetach() {
        super.onDetach();

        suggestClient = null;
        suggestionLoaderCallbacks = null;
        inputMethodManager = null;
        autoCompleteAdapter = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        mainView = inflater.inflate(R.layout.search_auto_complete, container, false);

        // Intercept clicks on the main view to deactivate the search bar.
        mainView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                transitionToWaiting();
            }
        });

        searchBar = mainView.findViewById(R.id.search_bar);
        editText = (EditText) mainView.findViewById(R.id.search_bar_edit_text);

        final View.OnClickListener transitionToRunningListener = new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                transitionToRunning();
            }
        };
        searchBar.setOnClickListener(transitionToRunningListener);
        editText.setOnClickListener(transitionToRunningListener);

        // Attach a listener for the "search" key on the keyboard.
        editText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                final Bundle args = new Bundle();
                args.putString(KEY_SEARCH_TERM, s.toString());
                getLoaderManager().restartLoader(LOADER_ID_SUGGESTION, args, suggestionLoaderCallbacks);
            }
        });
        editText.setOnEditorActionListener(this);

        clearButton = (Button) mainView.findViewById(R.id.search_bar_clear_button);
        clearButton.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                editText.setText("");
            }
        });

        suggestionDropdown = (ListView) mainView.findViewById(R.id.auto_complete_dropdown);
        suggestionDropdown.setAdapter(autoCompleteAdapter);

        // Attach listener for tapping on a suggestion.
        suggestionDropdown.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final String query = (String) suggestionDropdown.getItemAtPosition(position);
                startSearch(query);
            }
        });

        return mainView;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        searchBar = null;
        editText = null;
        clearButton = null;

        if (null != suggestionDropdown) {
            suggestionDropdown.setOnItemClickListener(null);
            suggestionDropdown.setAdapter(null);
            suggestionDropdown = null;
        }
    }

    /**
     * Handler for the "search" button on the keyboard.
     */
    @Override
    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
        if (actionId == EditorInfo.IME_ACTION_SEARCH) {
            startSearch(v.getText().toString());
            return true;
        }
        return false;
    }

    /**
     * Send a search intent and put the widget into waiting.
     */
    private void startSearch(String queryString) {
        if (getActivity() instanceof AcceptsSearchQuery) {
            editText.setText(queryString);
            editText.setSelection(queryString.length());
            transitionToWaiting();
            ((AcceptsSearchQuery) getActivity()).onSearch(queryString);
        } else {
            throw new RuntimeException("Parent activity does not implement AcceptsSearchQuery.");
        }
    }

    private void transitionToWaiting() {
        if (state == State.WAITING) {
            return;
        }

        setEditTextFocusable(false);
        mainView.setClickable(false);

        suggestionDropdown.setVisibility(View.GONE);
        clearButton.setVisibility(View.GONE);

        state = State.WAITING;
    }

    private void transitionToRunning() {
        if (state == State.RUNNING) {
            return;
        }

        setEditTextFocusable(true);
        mainView.setClickable(true);

        suggestionDropdown.setVisibility(View.VISIBLE);
        clearButton.setVisibility(View.VISIBLE);

        state = State.RUNNING;
    }

    private void setEditTextFocusable(boolean focusable) {
        editText.setFocusable(focusable);
        editText.setFocusableInTouchMode(focusable);

        if (focusable) {
            editText.requestFocus();
            inputMethodManager.showSoftInput(editText, InputMethodManager.SHOW_IMPLICIT);
        } else {
            editText.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(editText.getWindowToken(), 0);
        }
    }

    @Override
    public void onJumpTap(String suggestion) {
        editText.setText(suggestion);
        // Move cursor to end of search input.
        editText.setSelection(suggestion.length());
    }

    private class SuggestionLoaderCallbacks implements LoaderManager.LoaderCallbacks<List<String>> {
        @Override
        public Loader<List<String>> onCreateLoader(int id, Bundle args) {
            // suggestClient is set to null in onDestroyView(), so using it
            // safely here relies on the fact that onCreateLoader() is called
            // synchronously in restartLoader().
            return new SuggestionAsyncLoader(getActivity(), suggestClient, args.getString(KEY_SEARCH_TERM));
        }

        @Override
        public void onLoadFinished(Loader<List<String>> loader, List<String> suggestions) {
            autoCompleteAdapter.update(suggestions);
        }

        @Override
        public void onLoaderReset(Loader<List<String>> loader) {
            if (autoCompleteAdapter != null) {
                autoCompleteAdapter.update(null);
            }
        }
    }

    private static class SuggestionAsyncLoader extends AsyncTaskLoader<List<String>> {
        private final SuggestClient suggestClient;
        private final String searchTerm;
        private List<String> suggestions;

        public SuggestionAsyncLoader(Context context, SuggestClient suggestClient, String searchTerm) {
            super(context);
            this.suggestClient = suggestClient;
            this.searchTerm = searchTerm;
            this.suggestions = null;
        }

        @Override
        public List<String> loadInBackground() {
            return suggestClient.query(searchTerm);
        }

        @Override
        public void deliverResult(List<String> suggestions) {
            this.suggestions = suggestions;

            if (isStarted()) {
                super.deliverResult(suggestions);
            }
        }

        @Override
        protected void onStartLoading() {
            if (suggestions != null) {
                deliverResult(suggestions);
            }

            if (takeContentChanged() || suggestions == null) {
                forceLoad();
            }
        }

        @Override
        protected void onStopLoading() {
            cancelLoad();
        }

        @Override
        protected void onReset() {
            super.onReset();

            onStopLoading();
            suggestions = null;
        }
    }
}
