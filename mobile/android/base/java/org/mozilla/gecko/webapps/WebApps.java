/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapps;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.widget.ListView;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.prompts.Prompt;
import org.mozilla.gecko.prompts.PromptListItem;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * 10 predefined slots for homescreen webapps, in LauncherActivity
 * launched webapps will be given an index (via WebAppIndexer) that
 * points to one of these class names
 **/

public final class WebApps {
    public static class WebApp0 extends WebAppActivity { }
    public static class WebApp1 extends WebAppActivity { }
    public static class WebApp2 extends WebAppActivity { }
    public static class WebApp3 extends WebAppActivity { }
    public static class WebApp4 extends WebAppActivity { }
    public static class WebApp5 extends WebAppActivity { }
    public static class WebApp6 extends WebAppActivity { }
    public static class WebApp7 extends WebAppActivity { }
    public static class WebApp8 extends WebAppActivity { }
    public static class WebApp9 extends WebAppActivity { }

    public static void openInFennec(final Uri uri, final Context context) {
        ThreadUtils.assertOnUiThread();

        final Prompt prompt = new Prompt(context, new Prompt.PromptCallback() {
            @Override
            public void onPromptFinished(final GeckoBundle result) {

                final int itemId = result.getInt("button", -1);

                if (itemId == -1) {
                    // this is the error case, we shouldn't have this situation.
                    return;
                }
                Intent intent = new Intent(context, BrowserApp.class);
                // BrowserApp's onNewIntent will check action so below is required
                intent.setAction(Intent.ACTION_VIEW);
                intent.setData(uri);
                intent.setPackage(context.getPackageName());
                context.startActivity(intent);

            }
        });

        final PromptListItem[] items = new PromptListItem[1];
        items[0] = new PromptListItem(context.getResources().getString(R.string.overlay_share_open_browser_btn_label));
        prompt.show("", "", items, ListView.CHOICE_MODE_NONE);

    }

    @Nullable
    public static Uri getValidURL(@NonNull String urlString) {
        final Uri uri = Uri.parse(urlString);
        if (uri == null) {
            return null;
        }
        final String scheme = uri.getScheme();
        // currently we only support http and https to open in Firefox
        if (scheme.equals("http") || scheme.equals("https")) {
            return uri;
        } else {
            return null;
        }
    }
}
