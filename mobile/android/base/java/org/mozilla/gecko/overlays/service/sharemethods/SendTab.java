/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.service.sharemethods;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.TabsAccessor;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.overlays.OverlayConstants;
import org.mozilla.gecko.overlays.service.ShareData;
import org.mozilla.gecko.sync.CommandProcessor;
import org.mozilla.gecko.sync.CommandRunner;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.SyncConfiguration;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * ShareMethod implementation to handle Sync's "Send tab to device" mechanism.
 * See OverlayConstants for documentation of OverlayIntentHandler service intent API (which is how
 * this class is chiefly interacted with).
 */
public class SendTab extends ShareMethod {
    private static final String LOGTAG = "GeckoSendTab";

    // Key used in the extras Bundle in the share intent used for a send tab ShareMethod.
    public static final String SEND_TAB_TARGET_DEVICES = "SEND_TAB_TARGET_DEVICES";

    // Key used in broadcast intent from SendTab ShareMethod specifying available RemoteClients.
    public static final String EXTRA_REMOTE_CLIENT_RECORDS = "RECORDS";

    // The intent we should dispatch when the button for this ShareMethod is tapped, instead of
    // taking the normal action (e.g., "Set up Sync!")
    public static final String OVERRIDE_INTENT = "OVERRIDE_INTENT";

    private Set<String> validGUIDs;

    // A TabSender appropriate to the account type we're connected to.
    private TabSender tabSender;

    @Override
    public Result handle(ShareData shareData) {
        if (shareData.extra == null) {
            Log.e(LOGTAG, "No target devices specified!");

            // Retrying with an identical lack of devices ain't gonna fix it...
            return Result.PERMANENT_FAILURE;
        }

        String[] targetGUIDs = ((Bundle) shareData.extra).getStringArray(SEND_TAB_TARGET_DEVICES);

        // Ensure all target GUIDs are devices we actually know about.
        if (!validGUIDs.containsAll(Arrays.asList(targetGUIDs))) {
            // Find the set of invalid GUIDs to provide a nice error message.
            Log.e(LOGTAG, "Not all provided GUIDs are real devices:");
            for (String targetGUID : targetGUIDs) {
                if (!validGUIDs.contains(targetGUID)) {
                    Log.e(LOGTAG, "Invalid GUID: " + targetGUID);
                }
            }

            return Result.PERMANENT_FAILURE;
        }

        Log.i(LOGTAG, "Send tab handler invoked.");

        final CommandProcessor processor = CommandProcessor.getProcessor();

        final String accountGUID = tabSender.getAccountGUID();
        Log.d(LOGTAG, "Retrieved local account GUID '" + accountGUID + "'.");

        if (accountGUID == null) {
            Log.e(LOGTAG, "Cannot determine account GUID");

            // It's not completely out of the question that a background sync might come along and
            // fix everything for us...
            return Result.TRANSIENT_FAILURE;
        }

        // Queue up the share commands for each destination device.
        // Remember that ShareMethod.handle is always run on the background thread, so the database
        // access here is of no concern.
        for (int i = 0; i < targetGUIDs.length; i++) {
            processor.sendURIToClientForDisplay(shareData.url, targetGUIDs[i], shareData.title, accountGUID, context);
        }

        // Request an immediate sync to push these new commands to the network ASAP.
        Log.i(LOGTAG, "Requesting immediate clients stage sync.");
        tabSender.sync();

        return Result.SUCCESS;
        // ... Probably.
    }

    /**
     * Get an Intent suitable for broadcasting the UI state of this ShareMethod.
     * The caller shall populate the intent with the actual state.
     */
    private Intent getUIStateIntent() {
        Intent uiStateIntent = new Intent(OverlayConstants.SHARE_METHOD_UI_EVENT);
        uiStateIntent.putExtra(OverlayConstants.EXTRA_SHARE_METHOD, (Parcelable) Type.SEND_TAB);
        return uiStateIntent;
    }

    /**
     * Broadcast the given intent to any UIs that may be listening.
     */
    private void broadcastUIState(Intent uiStateIntent) {
        LocalBroadcastManager.getInstance(context).sendBroadcast(uiStateIntent);
    }

    /**
     * Load the state of the user's Firefox Sync accounts and broadcast it to any registered
     * listeners. This will cause any UIs that may exist that depend on this information to update.
     */
    public SendTab(Context aContext) {
        super(aContext);
        // Initialise the UI state intent...

        // Determine if the user has a new or old style sync account and load the available sync
        // clients for it.
        final AccountManager accountManager = AccountManager.get(context);
        final Account[] fxAccounts = accountManager.getAccountsByType(FxAccountConstants.ACCOUNT_TYPE);

        if (fxAccounts.length > 0) {
            final AndroidFxAccount fxAccount = new AndroidFxAccount(context, fxAccounts[0]);
            if (fxAccount.getState().getNeededAction() != State.Action.None) {
                // We have a Firefox Account, but it's definitely not able to send a tab
                // right now. Redirect to the status activity.
                Log.w(LOGTAG, "Firefox Account named like " + fxAccount.getObfuscatedEmail() +
                              " needs action before it can send a tab; redirecting to status activity.");

                setOverrideIntentAction(FxAccountConstants.ACTION_FXA_STATUS);
                return;
            }

            tabSender = new FxAccountTabSender(fxAccount);

            updateClientList(tabSender);

            Log.i(LOGTAG, "Allowing tab send for Firefox Account.");
            registerDisplayURICommand();
            return;
        }

        // Have registered UIs offer to set up a Firefox Account.
        setOverrideIntentAction(FxAccountConstants.ACTION_FXA_GET_STARTED);
    }

    /**
     * Load the list of Sync clients that are not this device using the given TabSender.
     */
    private void updateClientList(TabSender tabSender) {
        Collection<RemoteClient> otherClients = getOtherClients(tabSender);

        // Put the list of RemoteClients into the uiStateIntent and broadcast it.
        RemoteClient[] records = new RemoteClient[otherClients.size()];
        records = otherClients.toArray(records);

        validGUIDs = new HashSet<>();

        for (RemoteClient client : otherClients) {
            validGUIDs.add(client.guid);
        }

        if (validGUIDs.isEmpty()) {
            // Guess we'd better override. We have no clients.
            // This does the broadcast for us.
            setOverrideIntentAction(FxAccountConstants.ACTION_FXA_GET_STARTED);
            return;
        }

        Intent uiStateIntent = getUIStateIntent();
        uiStateIntent.putExtra(EXTRA_REMOTE_CLIENT_RECORDS, records);
        broadcastUIState(uiStateIntent);
    }

    /**
     * Record our intention to redirect the user to a different activity when they attempt to share
     * with us, usually because we found something wrong with their Sync account (a need to login,
     * register, etc.)
     * This will be recorded in the OVERRIDE_INTENT field of the UI broadcast. Consumers should
     * dispatch this intent instead of attempting to share with this ShareMethod whenever it is
     * non-null.
     *
     * @param action to launch instead of invoking a share.
     */
    protected void setOverrideIntentAction(final String action) {
        Intent intent = new Intent(action);
        // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
        // the soft keyboard not being shown for the started activity. Why, Android, why?
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Intent uiStateIntent = getUIStateIntent();
        uiStateIntent.putExtra(OVERRIDE_INTENT, intent);

        broadcastUIState(uiStateIntent);
    }

    private static void registerDisplayURICommand() {
        final CommandProcessor processor = CommandProcessor.getProcessor();
        processor.registerCommand("displayURI", new CommandRunner(3) {
            @Override
            public void executeCommand(final GlobalSession session, List<String> args) {
                CommandProcessor.displayURI(args, session.getContext());
            }
        });
    }

    /**
     * @return A collection of unique remote clients sorted by most recently used.
     */
    protected Collection<RemoteClient> getOtherClients(final TabSender sender) {
        if (sender == null) {
            Log.w(LOGTAG, "No tab sender when fetching other client IDs.");
            return Collections.emptyList();
        }

        final BrowserDB browserDB = GeckoProfile.get(context).getDB();
        final TabsAccessor tabsAccessor = browserDB.getTabsAccessor();
        final Cursor remoteTabsCursor = tabsAccessor.getRemoteClientsByRecencyCursor(context);
        try {
            if (remoteTabsCursor.getCount() == 0) {
                return Collections.emptyList();
            }
            return tabsAccessor.getClientsWithoutTabsByRecencyFromCursor(remoteTabsCursor);
        } finally {
            remoteTabsCursor.close();
        }
    }

    /**
     * Inteface for interacting with Sync accounts. Used to hide the difference in implementation
     * between FXA and "old sync" accounts when sending tabs.
     */
    private interface TabSender {
        public static final String[] STAGES_TO_SYNC = new String[] { "clients", "tabs" };

        /**
         * @return Return null if the account isn't correctly initialized. Return
         *         the account GUID otherwise.
         */
        String getAccountGUID();

        /**
         * Sync this account, specifying only clients and tabs as the engines to sync.
         */
        void sync();
    }

    private static class FxAccountTabSender implements TabSender {
        private final AndroidFxAccount fxAccount;

        public FxAccountTabSender(AndroidFxAccount fxa) {
            fxAccount = fxa;
        }

        @Override
        public String getAccountGUID() {
            try {
                final SharedPreferences prefs = fxAccount.getSyncPrefs();
                return prefs.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null);
            } catch (Exception e) {
                Log.w(LOGTAG, "Could not get Firefox Account parameters or preferences; aborting.");
                return null;
            }
        }

        @Override
        public void sync() {
            fxAccount.requestImmediateSync(STAGES_TO_SYNC, null);
        }
    }
}
