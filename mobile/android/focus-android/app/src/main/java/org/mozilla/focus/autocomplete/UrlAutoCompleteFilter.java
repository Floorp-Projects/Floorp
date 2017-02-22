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

public class UrlAutoCompleteFilter implements InlineAutocompleteEditText.OnFilterListener {
    private List<String> domains;

    public UrlAutoCompleteFilter(Context context) {
        loadUrls(context);
    }

    @Override
    public void onFilter(String searchText, InlineAutocompleteEditText view) {
        if (domains == null || view == null) {
            return;
        }

        for (final String domain : domains) {
            final String wwwDomain = "www." + domain;
            if (wwwDomain.startsWith(searchText)) {
                view.onAutocomplete(wwwDomain);
                return;
            }

            if (domain.startsWith(searchText)) {
                view.onAutocomplete(domain);
                return;
            }
        }
    }

    private void loadUrls(Context context) {
        new AsyncTask<Resources, Void, List<String>>() {
            @Override
            protected List<String> doInBackground(Resources... resources) {
                try {
                    final BufferedReader reader = new BufferedReader(new InputStreamReader(
                            resources[0].openRawResource(R.raw.topdomains)));

                    final List<String> domains = new ArrayList<String>(460);

                    String line;
                    while ((line = reader.readLine()) != null) {
                        domains.add(line);
                    }

                    return domains;
                } catch (IOException e) {
                    // No autocomplete for you!
                    return null;
                }
            }

            @Override
            protected void onPostExecute(List<String> domains) {
                UrlAutoCompleteFilter.this.domains = domains;
            }
        }.execute(context.getResources());
    }
}
