/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.os.Bundle;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.GeckoRequest;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

public class CustomTabsActivity extends GeckoApp {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public int getLayout() {
        return R.layout.customtabs_activity;
    }

    @Override
    public void onBackPressed() {
        final Tabs tabs = Tabs.getInstance();
        final Tab tab = tabs.getSelectedTab();

        // Give Gecko a chance to handle the back press first, then fallback to the Java UI.
        GeckoAppShell.sendRequestToGecko(new GeckoRequest("Browser:OnBackPressed", null) {
            @Override
            public void onResponse(NativeJSObject nativeJSObject) {
                if (!nativeJSObject.getBoolean("handled")) {
                    // Default behavior is Gecko didn't prevent.
                    onDefault();
                }
            }

            @Override
            public void onError(NativeJSObject error) {
                // Default behavior is Gecko didn't prevent, via failure.
                onDefault();
            }

            // Return from Gecko thread, then back-press through the Java UI.
            private void onDefault() {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (tab.doBack()) {
                            return;
                        }

                        finish();
                    }
                });
            }
        });
    }
}
