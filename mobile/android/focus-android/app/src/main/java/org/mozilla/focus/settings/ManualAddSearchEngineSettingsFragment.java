/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import org.mozilla.focus.R;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.search.ManualAddSearchEnginePreference;
import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.SupportUtils;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

public class ManualAddSearchEngineSettingsFragment extends SettingsFragment {
    private static String LOGTAG = "ManualAddSearchEngine";

    // Set so the user doesn't have to wait *too* long. It's used twice: once for connecting and once for reading.
    private static int SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS = 4000;

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
            case R.id.learn_more:
                final Context context = getActivity();
                final String url = SupportUtils.getSumoURLForTopic(context, "add-search-engine");

                final String title = ((AppCompatActivity) getActivity()).getSupportActionBar().getTitle().toString();
                final Intent intent = InfoActivity.getIntentFor(context, url, title);
                context.startActivity(intent);

                TelemetryWrapper.addSearchEngineLearnMoreEvent();
                return true;

            case R.id.menu_save_search_engine:
                final View rootView = getView();
                final String engineName = ((EditText) rootView.findViewById(R.id.edit_engine_name)).getText().toString();
                final String searchQuery = ((EditText) rootView.findViewById(R.id.edit_search_string)).getText().toString();

                final ManualAddSearchEnginePreference pref = (ManualAddSearchEnginePreference) findPreference(getString(R.string.pref_key_manual_add_search_engine));
                final boolean engineValid = pref.validateEngineNameAndShowError(engineName);
                final boolean searchValid = pref.validateSearchQueryAndShowError(searchQuery);
                final boolean isPartialSuccess = engineValid && searchValid;

                if (isPartialSuccess) {
                    // Hide the keyboard because:
                    // - It's awkward to show the keyboard while waiting for a response
                    // - We want it hidden when we return to the previous screen (on success)
                    // - An expanded keyboard hides the success snackbar
                    ViewUtils.hideKeyboard(rootView);
                    setUiIsValidatingAsync(true, item);
                    new ValidateSearchEngineAsyncTask(this, item, engineName, searchQuery).execute();
                } else {
                    TelemetryWrapper.saveCustomSearchEngineEvent(false);
                }
                return true;

            default:
                return super.onOptionsItemSelected(item);
        }
    }

    private void setUiIsValidatingAsync(final boolean isValidating, final MenuItem saveMenuItem) {
        final ManualAddSearchEnginePreference pref =
                (ManualAddSearchEnginePreference) findPreference(getString(R.string.pref_key_manual_add_search_engine));
        pref.setProgressViewShown(isValidating);

        saveMenuItem.setEnabled(!isValidating);
    }

    private static class ValidateSearchEngineAsyncTask extends AsyncTask<Void, Void, Boolean> {
        private final WeakReference<ManualAddSearchEngineSettingsFragment> thatWeakReference;
        private final WeakReference<MenuItem> saveMenuItemWeakReference;
        private final String engineName;
        private final String query;

        private ValidateSearchEngineAsyncTask(final ManualAddSearchEngineSettingsFragment that, final MenuItem saveMenuItem,
                final String engineName, final String query) {
            this.thatWeakReference = new WeakReference<>(that);
            this.saveMenuItemWeakReference = new WeakReference<>(saveMenuItem); // contains reference to Context.
            this.engineName = engineName;
            this.query = query;
        }

        @Override
        protected Boolean doInBackground(final Void... voids) {
            final boolean isValidSearchQuery = isValidSearchQueryURL(query);
            TelemetryWrapper.saveCustomSearchEngineEvent(isValidSearchQuery);
            return isValidSearchQuery;
        }

        @Override
        protected void onPostExecute(final Boolean isValidSearchQuery) {
            super.onPostExecute(isValidSearchQuery);

            final ManualAddSearchEngineSettingsFragment that = thatWeakReference.get();
            final MenuItem saveMenuItem = saveMenuItemWeakReference.get();
            if (that == null || saveMenuItem == null) {
                Log.d(LOGTAG, "Fragment or menu item no longer exists when search query validation async task returned.");
                return;
            }

            if (isValidSearchQuery) {
                final SharedPreferences sharedPreferences = that.getSearchEngineSharedPreferences();
                SearchEngineManager.addSearchEngine(sharedPreferences, that.getActivity(), engineName, query);
                Snackbar.make(that.getView(), R.string.search_add_confirmation, Snackbar.LENGTH_SHORT).show();
                that.getFragmentManager().popBackStack();
            } else {
                showServerError(that);
            }

            that.setUiIsValidatingAsync(false, saveMenuItem);
        }

        private void showServerError(final ManualAddSearchEngineSettingsFragment that) {
            final ManualAddSearchEnginePreference pref = (ManualAddSearchEnginePreference) that.findPreference(
                    that.getString(R.string.pref_key_manual_add_search_engine));
            pref.setSearchQueryErrorText(that.getString(R.string.error_hostLookup_title));
        }
    }

    @WorkerThread // makes network request.
    private static boolean isValidSearchQueryURL(final String query) {
        // TODO: we should share the code to substitute and normalize the search string (see SearchEngine.buildSearchUrl).
        final String encodedTestQuery = Uri.encode("test");
        final String normalizedSearchURLStr = UrlUtils.normalize(query);
        final String searchURLStr = normalizedSearchURLStr.replaceAll("%s", encodedTestQuery);

        final URL searchURL;
        try {
            searchURL = new URL(searchURLStr);
        } catch (final MalformedURLException e) {
            // Don't log exception to avoid leaking URL.
            Log.d(LOGTAG, "Malformed URL: returning invalid search query");
            return false;
        }

        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) searchURL.openConnection();
            connection.setConnectTimeout(SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS);
            connection.setReadTimeout(SEARCH_QUERY_VALIDATION_TIMEOUT_MILLIS);

            // Checking for 200 is not perfect - what if a search engine redirects us? -
            // but we're in a rush and it's good enough for now. One notable case this fails:
            // if sites redirect to https, e.g. "duckduckgo.com/?q=%s" redirects to https and thus fails.
            return connection.getResponseCode() == 200;

        } catch (final IOException e) {
            // Don't log exception to avoid leaking URL.
            Log.d(LOGTAG, "Failure to get response code from server: returning invalid search query");
            return false;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }
}
