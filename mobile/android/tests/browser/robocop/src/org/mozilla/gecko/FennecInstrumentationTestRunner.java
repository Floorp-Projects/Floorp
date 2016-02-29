/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.KeyguardManager;
import android.content.Context;
import android.os.Bundle;
import android.os.PowerManager;
import android.test.InstrumentationTestRunner;
import android.util.Log;

import static android.os.PowerManager.ACQUIRE_CAUSES_WAKEUP;
import static android.os.PowerManager.FULL_WAKE_LOCK;
import static android.os.PowerManager.ON_AFTER_RELEASE;

public class FennecInstrumentationTestRunner extends InstrumentationTestRunner {
    private static Bundle sArguments;
    private PowerManager.WakeLock wakeLock;
    private KeyguardManager.KeyguardLock keyguardLock;

    @Override
    public void onCreate(Bundle arguments) {
        sArguments = arguments;
        if (sArguments == null) {
            Log.e("Robocop", "FennecInstrumentationTestRunner.onCreate got null bundle");
        }
        super.onCreate(arguments);
    }

    // unfortunately we have to make this static because test classes that don't extend
    // from ActivityInstrumentationTestCase2 can't get a reference to this class.
    public static Bundle getFennecArguments() {
        if (sArguments == null) {
            Log.e("Robocop", "FennecInstrumentationTestCase.getFennecArguments returns null bundle");
        }
        return sArguments;
    }

    @Override
    public void onStart() {
        final Context context = getContext(); // The Robocop package itself has DISABLE_KEYGUARD and WAKE_LOCK.
        if (context != null) {
            try {
                String name = FennecInstrumentationTestRunner.class.getSimpleName();
                // Unlock the device so that the tests can input keystrokes.
                final KeyguardManager keyguard = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
                // Deprecated in favour of window flags, which aren't appropriate here.
                keyguardLock = keyguard.newKeyguardLock(name);
                keyguardLock.disableKeyguard();

                // Wake up the screen.
                final PowerManager power = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
                wakeLock = power.newWakeLock(FULL_WAKE_LOCK | ACQUIRE_CAUSES_WAKEUP | ON_AFTER_RELEASE, name);
                wakeLock.acquire();
            } catch (SecurityException e) {
                Log.w("GeckoInstTestRunner", "Got SecurityException: not disabling keyguard and not taking wakelock.");
            }
        } else {
            Log.w("GeckoInstTestRunner", "Application target context is null: not disabling keyguard and not taking wakelock.");
        }

        super.onStart();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (wakeLock != null) {
            wakeLock.release();
        }
        if (keyguardLock != null) {
            // Deprecated in favour of window flags, which aren't appropriate here.
            keyguardLock.reenableKeyguard();
        }
    }
}
