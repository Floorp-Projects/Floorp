/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

public class SafeIntentUtils {
    private static final String LOGTAG = "GeckoContextUtils";

    public static Bundle getBundleExtra(final Intent intent, final String name) {
        return new SafeIntent(intent).getBundleExtra(name);
    }

    public static String getStringExtra(final Intent intent, final String name) {
        return new SafeIntent(intent).getStringExtra(name);
    }

    public static boolean getBooleanExtra(Intent intent, String name, boolean defaultValue) {
        return new SafeIntent(intent).getBooleanExtra(name, defaultValue);
    }

    /**
     * External applications can pass values into Intents that can cause us to crash: in defense,
     * we wrap {@link Intent} and catch the exceptions they may force us to throw. See bug 1090385
     * for more.
     */
    public static class SafeIntent {
        private final Intent intent;

        public SafeIntent(final Intent intent) {
            this.intent = intent;
        }

        public boolean getBooleanExtra(final String name, final boolean defaultValue) {
            try {
                return intent.getBooleanExtra(name, defaultValue);
            } catch (OutOfMemoryError e) {
                Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?");
                return defaultValue;
            } catch (RuntimeException e) {
                Log.w(LOGTAG, "Couldn't get intent extras.", e);
                return defaultValue;
            }
        }

        public String getStringExtra(final String name) {
            try {
                return intent.getStringExtra(name);
            } catch (OutOfMemoryError e) {
                Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?");
                return null;
            } catch (RuntimeException e) {
                Log.w(LOGTAG, "Couldn't get intent extras.", e);
                return null;
            }
        }

        public Bundle getBundleExtra(final String name) {
            try {
                return intent.getBundleExtra(name);
            } catch (OutOfMemoryError e) {
                Log.w(LOGTAG, "Couldn't get intent extras: OOM. Malformed?");
                return null;
            } catch (RuntimeException e) {
                Log.w(LOGTAG, "Couldn't get intent extras.", e);
                return null;
            }
        }

        public String getAction() {
            return intent.getAction();
        }

        public String getDataString() {
            try {
                return intent.getDataString();
            } catch (OutOfMemoryError e) {
                Log.w(LOGTAG, "Couldn't get intent data string: OOM. Malformed?");
                return null;
            } catch (RuntimeException e) {
                Log.w(LOGTAG, "Couldn't get intent data string.", e);
                return null;
            }
        }

        public Uri getData() {
            try {
                return intent.getData();
            } catch (OutOfMemoryError e) {
                Log.w(LOGTAG, "Couldn't get intent data: OOM. Malformed?");
                return null;
            } catch (RuntimeException e) {
                Log.w(LOGTAG, "Couldn't get intent data.", e);
                return null;
            }
        }

        public Intent getUnsafe() {
            return intent;
        }
    }
}
