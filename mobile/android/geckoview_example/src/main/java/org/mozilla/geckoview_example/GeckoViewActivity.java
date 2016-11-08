/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import org.mozilla.gecko.BaseGeckoInterface;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.PrefsHelper;

import static org.mozilla.gecko.GeckoView.setGeckoInterface;

public class GeckoViewActivity extends Activity {
    private static final String LOGTAG = "GeckoViewActivity";

    GeckoView mGeckoView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setGeckoInterface(new BaseGeckoInterface(getApplicationContext()));

        setContentView(R.layout.geckoview_activity);

        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);
        mGeckoView.setChromeDelegate(new MyGeckoViewChrome());
        mGeckoView.setContentDelegate(new MyGeckoViewContent());
    }

    @Override
    protected void onStart() {
        super.onStart();

        final GeckoProfile profile = GeckoProfile.get(getApplicationContext());

        GeckoThread.init(profile, /* args */ null, /* action */ null, /* debugging */ false);
        GeckoThread.launch();
    }

    private class MyGeckoViewChrome implements GeckoView.ChromeDelegate {
        @Override
        public void onReady(GeckoView view) {
            Log.i(LOGTAG, "Gecko is ready");
            // // Inject a script that adds some code to the content window
            // mGeckoView.importScript("resource://android/assets/script.js");

            // Set up remote debugging to a port number
            PrefsHelper.setPref("layers.dump", true);
            PrefsHelper.setPref("devtools.debugger.remote-port", 6000);
            PrefsHelper.setPref("devtools.debugger.unix-domain-socket", "");
            PrefsHelper.setPref("devtools.debugger.remote-enabled", true);

            // The Gecko libraries have finished loading and we can use the rendering engine.
            // Let's add a browser (required) and load a page into it.
            // mGeckoView.addBrowser(getResources().getString(R.string.default_url));
        }

        @Override
        public void onAlert(GeckoView view, GeckoView.Browser browser, String message, GeckoView.PromptResult result) {
            Log.i(LOGTAG, "Alert!");
            result.confirm();
            Toast.makeText(getApplicationContext(), message, Toast.LENGTH_LONG).show();
        }

        @Override
        public void onConfirm(GeckoView view, GeckoView.Browser browser, String message, final GeckoView.PromptResult result) {
            Log.i(LOGTAG, "Confirm!");
            new AlertDialog.Builder(GeckoViewActivity.this)
                .setTitle("javaScript dialog")
                .setMessage(message)
                .setPositiveButton(android.R.string.ok,
                                   new DialogInterface.OnClickListener() {
                                       public void onClick(DialogInterface dialog, int which) {
                                           result.confirm();
                                       }
                                   })
                .setNegativeButton(android.R.string.cancel,
                                   new DialogInterface.OnClickListener() {
                                       public void onClick(DialogInterface dialog, int which) {
                                           result.cancel();
                                       }
                                   })
                .create()
                .show();
        }

        @Override
        public void onPrompt(GeckoView view, GeckoView.Browser browser, String message, String defaultValue, GeckoView.PromptResult result) {
            result.cancel();
        }

        @Override
        public void onDebugRequest(GeckoView view, GeckoView.PromptResult result) {
            Log.i(LOGTAG, "Remote Debug!");
            result.confirm();
        }

        @Override
        public void onScriptMessage(GeckoView view, Bundle data, GeckoView.MessageResult result) {
            Log.i(LOGTAG, "Got Script Message: " + data.toString());
            String type = data.getString("type");
            if ("fetch".equals(type)) {
                Bundle ret = new Bundle();
                ret.putString("name", "Mozilla");
                ret.putString("url", "https://mozilla.org");
                result.success(ret);
            }
        }
    }

    private class MyGeckoViewContent implements GeckoView.ContentDelegate {
        @Override
        public void onPageStart(GeckoView view, GeckoView.Browser browser, String url) {

        }

        @Override
        public void onPageStop(GeckoView view, GeckoView.Browser browser, boolean success) {

        }

        @Override
        public void onPageShow(GeckoView view, GeckoView.Browser browser) {

        }

        @Override
        public void onReceivedTitle(GeckoView view, GeckoView.Browser browser, String title) {
            Log.i(LOGTAG, "Received a title: " + title);
        }

        @Override
        public void onReceivedFavicon(GeckoView view, GeckoView.Browser browser, String url, int size) {
            Log.i(LOGTAG, "Received a favicon URL: " + url);
        }
    }
}
