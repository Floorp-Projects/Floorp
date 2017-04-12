/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.autocomplete;

import android.content.Context;
import android.content.res.Resources;
import android.os.AsyncTask;

import org.mozilla.focus.R;
import org.mozilla.focus.widget.InlineAutocompleteEditText;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class UrlAutoCompleteFilter implements InlineAutocompleteEditText.OnFilterListener {
    private List<String> domains;

    public UrlAutoCompleteFilter(Context context) {
        loadUrls(context);
    }

    /**
     * Our autocomplete list is all lower case, however the search text might be mixed case.
     * Our autocomplete EditText code does more string comparison, which fails if the suggestion
     * doesn't exactly match searchText (ie. if casing differs). It's simplest to just build a suggestion
     * that exactly matches the search text - which is what this method is for:
     */
    private static String prepareAutocompleteResult(final String rawSearchText, final String lowerCaseResult) {
        return rawSearchText + lowerCaseResult.substring(rawSearchText.length());
    }

    @Override
    public void onFilter(final String rawSearchText, InlineAutocompleteEditText view) {
        if (domains == null || view == null) {
            return;
        }

        // Search terms are all lowercase already, we just need to lowercase the search text
        final String searchText = rawSearchText.toLowerCase(Locale.US);

        for (final String domain : domains) {
            final String wwwDomain = "www." + domain;
            if (wwwDomain.startsWith(searchText)) {
                view.onAutocomplete(prepareAutocompleteResult(rawSearchText, wwwDomain));
                return;
            }

            if (domain.startsWith(searchText)) {
                view.onAutocomplete(prepareAutocompleteResult(rawSearchText, domain));
                return;
            }
        }
    }

    private void loadUrls(Context context) {
        new AsyncTask<Resources, Void, List<String>>() {
            @Override
            protected List<String> doInBackground(Resources... resources) {
                final BufferedReader reader = new BufferedReader(new InputStreamReader(
                        resources[0].openRawResource(R.raw.topdomains)));

                try {
                    final List<String> domains = new ArrayList<String>(460);

                    String line;
                    while ((line = reader.readLine()) != null) {
                        domains.add(line);
                    }

                    return domains;
                } catch (IOException e) {
                    // No autocomplete for you!
                    return null;
                } finally {
                    try {
                        reader.close();
                    } catch (IOException e) {
                        // There's really nothing we can do now.
                    }
                }
            }

            @Override
            protected void onPostExecute(List<String> domains) {
                UrlAutoCompleteFilter.this.domains = domains;
            }
        }.execute(context.getResources());
    }
}
