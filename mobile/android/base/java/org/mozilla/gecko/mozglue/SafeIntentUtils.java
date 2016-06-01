/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue;

import android.content.Intent;
import android.os.Bundle;
import org.mozilla.gecko.util.SafeIntent;

public class SafeIntentUtils {

    public static Bundle getBundleExtra(final Intent intent, final String name) {
        return new SafeIntent(intent).getBundleExtra(name);
    }

    public static String getStringExtra(final Intent intent, final String name) {
        return new SafeIntent(intent).getStringExtra(name);
    }

    public static boolean getBooleanExtra(Intent intent, String name, boolean defaultValue) {
        return new SafeIntent(intent).getBooleanExtra(name, defaultValue);
    }

}
