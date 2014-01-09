/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class WebAppDispatcher extends Activity {
    private static final String LOGTAG = "GeckoWebAppDispatcher";

    @Override
    protected void onCreate(Bundle bundle) {
        super.onCreate(bundle);

        WebAppAllocator allocator = WebAppAllocator.getInstance(getApplicationContext());

        if (bundle == null) {
            bundle = getIntent().getExtras();
        }

        String packageName = bundle.getString("packageName");

        int index = allocator.getIndexForApp(packageName);
        boolean isInstalled = index >= 0;
        if (!isInstalled) {
            index = allocator.findOrAllocatePackage(packageName);
        }

        // Copy the intent, without interfering with it.
        Intent intent = new Intent(getIntent());

        // Only change it's destination.
        intent.setClassName(getApplicationContext(), getPackageName() + ".WebApps$WebApp" + index);

        // If and only if we haven't seen this before.
        intent.putExtra("isInstalled", isInstalled);

        startActivity(intent);
    }
}
