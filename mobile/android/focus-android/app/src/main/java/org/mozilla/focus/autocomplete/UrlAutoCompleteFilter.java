/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.AsyncTask;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.widget.InlineAutocompleteEditText;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Locale;
import java.util.Set;

public class UrlAutoCompleteFilter implements InlineAutocompleteEditText.OnFilterListener {
    private static final String LOG_TAG = "UrlAutoCompleteFilter";

    private Set<String> domains;

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

    @VisibleForTesting void onDomainsLoaded(Set<String> domains) {
        this.domains = domains;
    }

    public void loadDomainsInBackground(final Context context) {
        new AsyncTask<Resources, Void, Set<String>>() {
            @Override
            protected Set<String> doInBackground(Resources... resources) {
                final Set<String> domains = new LinkedHashSet<String>();
                final Set<String> availableLists = getAvailableDomainLists(context);

                // First load the country specific lists following the default locale order
                for (final String country : Locales.getCountriesInDefaultLocaleList()) {
                    if (availableLists.contains(country)) {
                        loadDomainsForLanguage(context, domains, country);
                    }
                }

                // And then add domains from the global list
                loadDomainsForLanguage(context, domains, "global");

                return domains;
            }

            @Override
            protected void onPostExecute(Set<String> domains) {
                onDomainsLoaded(domains);
            }
        }.execute(context.getResources());
    }

    private Set<String> getAvailableDomainLists(Context context) {
        final Set<String> availableDomains = new HashSet<>();

        final AssetManager assetManager = context.getAssets();

        try {
            Collections.addAll(availableDomains, assetManager.list("domains"));
        } catch (IOException e) {
            Log.w(LOG_TAG, "Could not list domain list directory");
        }

        return availableDomains;
    }

    private void loadDomainsForLanguage(Context context, Set<String> domains, String country) {
        final AssetManager assetManager = context.getAssets();

        try (final BufferedReader reader = new BufferedReader(new InputStreamReader(
                assetManager.open("domains/" + country), StandardCharsets.UTF_8))) {

            String line;
            while ((line = reader.readLine()) != null) {
                domains.add(line);
            }
        } catch (IOException e) {
            Log.w(LOG_TAG, "Could not load domain list: " + country);
        }
    }
}
