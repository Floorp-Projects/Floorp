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

/**
 * Activity for receiving and processing an ACTION_PROCESS_TEXT intent.
 */
public class TextActionActivity extends Activity {
    @RequiresApi(api = Build.VERSION_CODES.M)
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final String searchText = getIntent().getCharSequenceExtra(Intent.EXTRA_PROCESS_TEXT).toString();
        final String searchUrl = UrlUtils.createSearchUrl(this, searchText);

        final Intent intent = new Intent(this, MainActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.putExtra(MainActivity.EXTRA_TEXT_SELECTION, true);
        intent.setData(Uri.parse(searchUrl));

        startActivity(intent);

        finish();
    }
}
