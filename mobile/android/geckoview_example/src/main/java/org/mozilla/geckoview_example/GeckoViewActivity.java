/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.mozilla.gecko.BaseGeckoInterface;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.GeckoView;

import static org.mozilla.gecko.GeckoView.setGeckoInterface;

public class GeckoViewActivity extends Activity {
    private static final String LOGTAG = "GeckoViewActivity";
    private static final String DEFAULT_URL = "https://mozilla.org";

    /* package */ static final int REQUEST_FILE_PICKER = 1;

    private GeckoView mGeckoView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setGeckoInterface(new BaseGeckoInterface(this));

        setContentView(R.layout.geckoview_activity);

        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);
        mGeckoView.setContentListener(new MyGeckoViewContent());
        mGeckoView.setProgressListener(new MyGeckoViewProgress());

        final BasicGeckoViewPrompt prompt = new BasicGeckoViewPrompt();
        prompt.filePickerRequestCode = REQUEST_FILE_PICKER;
        mGeckoView.setPromptDelegate(prompt);

        final GeckoProfile profile = GeckoProfile.get(this);

        GeckoThread.initMainProcess(profile, /* args */ null, /* debugging */ false);
        GeckoThread.launch();

        loadFromIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent externalIntent) {
        Log.d(LOGTAG, "SNORP: onNewIntent: " + externalIntent.getData().toString());
        loadFromIntent(externalIntent);
    }

    private void loadFromIntent(Intent intent) {
        Uri u = intent.getData();
        if (u != null) {
            mGeckoView.loadUri(u.toString());
        } else {
            mGeckoView.loadUri(DEFAULT_URL);
        }
    }

    @Override
    protected void onActivityResult(final int requestCode, final int resultCode,
                                    final Intent data) {
        if (requestCode == REQUEST_FILE_PICKER) {
            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoView.getPromptDelegate();
            prompt.onFileCallbackResult(resultCode, data);
        } else {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    private class MyGeckoViewContent implements GeckoView.ContentListener {
        @Override
        public void onTitleChange(GeckoView view, String title) {
            Log.i(LOGTAG, "Content title changed to " + title);
        }
    }

    private class MyGeckoViewProgress implements GeckoView.ProgressListener {
        @Override
        public void onPageStart(GeckoView view, String url) {
            Log.i(LOGTAG, "Starting to load page at " + url);
        }

        @Override
        public void onPageStop(GeckoView view, boolean success) {
            Log.i(LOGTAG, "Stopping page load " + (success ? "successfully" : "unsuccessfully"));
        }

        @Override
        public void onSecurityChange(GeckoView view, int status) {
            String statusString;
            if ((status & STATE_IS_BROKEN) != 0) {
                statusString = "broken";
            } else if ((status & STATE_IS_SECURE) != 0) {
                statusString = "secure";
            } else if ((status & STATE_IS_INSECURE) != 0) {
                statusString = "insecure";
            } else {
                statusString = "unknown";
            }
            Log.i(LOGTAG, "Security status changed to " + statusString);
        }
    }
}
