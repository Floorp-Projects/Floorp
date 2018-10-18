/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import android.accounts.Account;
import android.content.Context;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Separated;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.sync.FxAccountNotificationManager;
import org.mozilla.gecko.fxa.sync.FxAccountSyncAdapter;
import org.mozilla.gecko.sync.ThreadPool;

import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

public class FirefoxAccountsUtils {
    private static final String LOG_TAG = "FirefoxAccountUtils";

    private enum PickleFileState {
        UNDETERMINED,
        PRESENT,
        ABSENT
    }

    // Synchronization note: we might have concurrent access to this method, and we'd really like it to run atomically.
    /* package-private */ static synchronized void optionallySeparateAccountsDuringFirstRun(Context context, Account[] accounts) {
        // A "first run" from this class' perspective might occur in two contexts:
        // - during a background sync
        // - during a foreground Fennec activity
        // In the former case, we rely on querying presence of a pickle file.
        // In the latter, we rely on the "first run uuid" (not available during background operation).
        final String firstRunSessionUUID = EnvironmentUtils.firstRunUUID(context);

        // Skip first run checks if we're not in the first run.
        if (firstRunSessionUUID == null) {
            Logger.debug(LOG_TAG, "Skipping first run account separation checks.");
            return;
        }

        for (Account account : accounts) {
            final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);

            final String currentAccountFirstRunScope = fxAccount.getUserData(
                    AndroidFxAccount.ACCOUNT_KEY_FIRST_RUN_SCOPE);

            // Foreground case:
            // If an account was created during the current first run telemetry session, don't touch it.
            if (firstRunSessionUUID.equals(currentAccountFirstRunScope)) {
                Logger.debug(LOG_TAG, "Account was created during current first run; not separating.");
                return;
            }

            // Otherwise:
            // Account is present while in "First Run" and it wasn't created during this "First Run".
            // Normally this indicates that application data was cleared in a signed-in browser.
            // More rarely, this state might indicate that account shouldn't be there at all (Bug 1429735).
            // Either way, we move this account into a Separated state, requiring the user to
            // re-connect the Account.
            Logger.info(LOG_TAG, "Separating account during first run.");
            separateAccountAndShowNotification(context, fxAccount);
        }
    }

    // Synchronization note: we're really not expecting concurrent access to this method, but just in case...
    public static synchronized boolean separateAccountIfPickleFileAbsent(Context context, AndroidFxAccount fxAccount) {
        final PickleFileState pickleFileState = queryPickleFileState(context);
        final boolean pickleFilePresent;
        switch (pickleFileState) {
            case PRESENT:
                pickleFilePresent = true;
                break;
            // Err on the side of caution if we fail to determine pickle file state.
            // We consider this kind of account separation a privacy/security feature, and so we bear the
            // marginal cost (due to low probability) of possibly separating an account in error.
            case UNDETERMINED:
            case ABSENT:
                pickleFilePresent = false;
                break;
            default:
                throw new IllegalStateException("Unexpected pickle file state: " + pickleFileState);
        }

        if (pickleFilePresent) {
            Logger.info(LOG_TAG, "Pickle file present; leaving account in its current state.");
            return false;
        }

        Logger.info(LOG_TAG, "Pickle file absent; separating the account.");
        separateAccountAndShowNotification(context, fxAccount);
        return true;
    }

    public static void separateAccountAndShowNotification(final Context context, final AndroidFxAccount fxAccount) {
        final State currentState = fxAccount.getState();
        // Avoid work if we're already in the correct state.
        if (!(currentState instanceof Separated)) {
            fxAccount.setState(currentState.makeSeparatedState());
        }
        final FxAccountNotificationManager notificationManager = new FxAccountNotificationManager(
                FxAccountSyncAdapter.NOTIFICATION_ID);
        notificationManager.update(context, fxAccount);
    }

    private static PickleFileState queryPickleFileState(final Context context) {
        final CountDownLatch latch = new CountDownLatch(1);
        final AtomicBoolean fileExists = new AtomicBoolean(false);

        ThreadPool.run(new Runnable() {
            @Override
            public void run() {
                try {
                    final File file = context.getFileStreamPath(FxAccountConstants.ACCOUNT_PICKLE_FILENAME);
                    fileExists.set(file.exists());
                } finally {
                    latch.countDown();
                }
            }
        });

        try {
            latch.await();
        } catch (InterruptedException e) {
            Logger.warn(LOG_TAG, "Interrupted while querying pickle file state");
            return PickleFileState.UNDETERMINED;
        }

        if (fileExists.get()) {
            return PickleFileState.PRESENT;
        }

        return PickleFileState.ABSENT;
    }
}
