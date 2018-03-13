/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.annotation.RequiresApi;

import org.mozilla.focus.utils.UrlUtils;

import mozilla.components.utils.SafeIntent;

/**
 * Activity for receiving and processing an ACTION_PROCESS_TEXT intent.
 */
public class TextActionActivity extends Activity {
    @RequiresApi(api = Build.VERSION_CODES.M)
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final SafeIntent intent = new SafeIntent(getIntent());

        final CharSequence searchTextCharSequence = intent.getCharSequenceExtra(Intent.EXTRA_PROCESS_TEXT);
        final String searchText;
        if (searchTextCharSequence != null) {
            searchText = searchTextCharSequence.toString();
        } else {
            searchText = "";
        }
        final String searchUrl = UrlUtils.createSearchUrl(this, searchText);

        final Intent searchIntent = new Intent(this, MainActivity.class);
        searchIntent.setAction(Intent.ACTION_VIEW);
        searchIntent.putExtra(MainActivity.EXTRA_TEXT_SELECTION, true);
        searchIntent.setData(Uri.parse(searchUrl));

        startActivity(searchIntent);

        finish();
    }
}
