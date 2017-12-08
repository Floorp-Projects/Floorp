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
import android.support.annotation.NonNull;
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

    /**
     * A reference to an active async task, if applicable, used to manage the task for lifecycle changes.
     * See {@link #onPause()} for details.
     */
    private @Nullable AsyncTask activeAsyncTask;
    private @Nullable MenuItem menuItemForActiveAsyncTask;

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
    public void onPause() {
        super.onPause();

        // This is a last minute change and we want to keep the async task management simple: onPause is the
        // first required callback for various lifecycle changes: a dialog is shown, the user
        // leaves the app, the app rotates, etc. To keep things simple, we do our AsyncTask management here,
        // before it gets more complex (e.g. reattaching the AsyncTask to a new fragment).
        //
        // We cancel the AsyncTask also to keep things simple: if the task is cancelled, it will:
        // - Likely end immediately and we don't need to handle it returning after the lifecycle changes
        // - Get onPostExecute scheduled on the UI thread, which must run after onPause (since it also runs on
        // the UI thread), and we check if the AsyncTask is cancelled there before we perform any other actions.
        if (activeAsyncTask != null) {
            activeAsyncTask.cancel(true);
            setUiIsValidatingAsync(false, menuItemForActiveAsyncTask);

            activeAsyncTask = null;
            menuItemForActiveAsyncTask = null;
        }
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
                    activeAsyncTask = new ValidateSearchEngineAsyncTask(this, engineName, searchQuery).execute();
                    menuItemForActiveAsyncTask = item;
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
        private final String engineName;
        private final String query;

        private ValidateSearchEngineAsyncTask(final ManualAddSearchEngineSettingsFragment that, final String engineName,
                final String query) {
            this.thatWeakReference = new WeakReference<>(that);
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

            if (isCancelled()) {
                Log.d(LOGTAG, "ValidateSearchEngineAsyncTask has been cancelled");
                return;
            }

            final ManualAddSearchEngineSettingsFragment that = thatWeakReference.get();
            if (that == null) {
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

            that.setUiIsValidatingAsync(false, that.menuItemForActiveAsyncTask);
            that.activeAsyncTask = null;
            that.menuItemForActiveAsyncTask = null;
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

        // We enforce HTTPS because some search engines will redirect http queries to https, which our
        // validation (a 200 check) will fail, and since most search engines should support HTTPS, we
        // try to be safer by defaulting to it.
        final String normalizedHttpsSearchURLStr = enforceHTTPS(UrlUtils.normalize(query));
        final String searchURLStr = normalizedHttpsSearchURLStr.replaceAll("%s", encodedTestQuery);

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
            // but we're in a rush and it's good enough for now.
            return connection.getResponseCode() == 200;

        } catch (final IOException e) {
            // Don't log exception to avoid leaking URL.
            Log.d(LOGTAG, "Failure to get response code from server: returning invalid search query");
            return false;
        } finally {
            if (connection != null) {
                try {
                    connection.getInputStream().close(); // HttpURLConnection.getResponseCode opens the InputStream.
                } catch (final IOException e) { } // Whatever.
                connection.disconnect();
            }
        }
    }

    private static String enforceHTTPS(@NonNull String input) {
        if (input.startsWith("https://")) {
            return input;
        } else if (input.startsWith("http://")) {
            return input.replace("http", "https");
        } else { // must be non-HTTP url, nothing to do.
            return input;
        }
    }
}
