/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.preference.DialogPreference;
import android.util.AttributeSet;

import org.mozilla.focus.R;

/**
 * Preference for setting the default search engine.
 */
public class SearchEnginePreference extends DialogPreference {
    @SuppressWarnings("unused") // Class referenced from XML
    public SearchEnginePreference(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init(context);
    }

    @SuppressWarnings("unused") // Class referenced from XML
    public SearchEnginePreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context);
    }

    @SuppressWarnings("unused") // Class referenced from XML
    public SearchEnginePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    @SuppressWarnings("unused") // Class referenced from XML
    public SearchEnginePreference(Context context) {
        super(context);
        init(context);
    }

    private void init(Context context) {
        setTitle(SearchEngineManager.getDefaultSearchEngine(context).getLabel());
    }

    @Override
    protected void onPrepareDialogBuilder(AlertDialog.Builder builder) {
        final SearchEngineAdapter adapter = new SearchEngineAdapter(getContext());

        builder.setTitle(R.string.preference_dialog_title_search_engine);

        builder.setAdapter(adapter, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                persistSearchEngine(adapter.getItem(which));

                dialog.dismiss();
            }
        });

        builder.setPositiveButton(null, null);
        builder.setNegativeButton(R.string.action_cancel, this);
    }

    private void persistSearchEngine(SearchEngine searchEngine) {
        setTitle(searchEngine.getLabel());

        // TODO: Store selection. This doesn't make any sense until we imported the actual search engines (#184).
    }
}
