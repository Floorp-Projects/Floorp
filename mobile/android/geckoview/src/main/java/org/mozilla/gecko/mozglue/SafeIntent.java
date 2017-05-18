/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This should be in util/, but is here because of build dependency issues.
package org.mozilla.gecko.mozglue;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import java.util.ArrayList;

/**
 * External applications can pass values into Intents that can cause us to crash: in defense,
 * we wrap {@link Intent} and catch the exceptions they may force us to throw. See bug 1090385
 * for more.
 */
public class SafeIntent {
    private static final String LOGTAG = "Gecko" + SafeIntent.class.getSimpleName();

    private final Intent intent;

    public SafeIntent(final Intent intent) {
        this.intent = intent;
    }

    public boolean hasExtra(String name) {
        try {
            return intent.hasExtra(name);
        } catch (OutOfMemoryError e) {
            Log.w(LOGTAG, "Couldn't determine if intent had an extra: OOM. Malformed?");
            return false;
        } catch (RuntimeException e) {
            Log.w(LOGTAG, "Couldn't determine if intent had an extra.", e);
            return false;
        }
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

    public int getIntExtra(final String name, final int defaultValue) {
        try {
            return intent.getIntExtra(name, defaultValue);
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

    public ArrayList<String> getStringArrayListExtra(final String name) {
        try {
            return intent.getStringArrayListExtra(name);
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
