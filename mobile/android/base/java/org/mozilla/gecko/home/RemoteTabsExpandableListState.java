/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.HashSet;
import java.util.Set;

import org.mozilla.gecko.util.PrefUtils;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

/**
 * Encapsulate visual state maintained by the Remote Tabs home panel.
 * <p>
 * This state should persist across database updates by Sync and the like. This
 * state could be stored in a separate "clients_metadata" table and served by
 * the Tabs provider, but that is heavy-weight for what we want to achieve. Such
 * a scheme would require either an expensive table join, or a tricky
 * co-ordination between multiple cursors. In contrast, this is easy and cheap
 * enough to do on the main thread.
 * <p>
 * This state is "per SharedPreferences" object. In practice, there should exist
 * one state object per Gecko Profile; since we can't change profiles without
 * killing our process, this can be a static singleton.
 */
public class RemoteTabsExpandableListState {
    private static final String PREF_COLLAPSED_CLIENT_GUIDS = "remote_tabs_collapsed_client_guids";
    private static final String PREF_HIDDEN_CLIENT_GUIDS = "remote_tabs_hidden_client_guids";
    private static final String PREF_SELECTED_CLIENT_GUID = "remote_tabs_selected_client_guid";

    protected final SharedPreferences sharedPrefs;

    // Synchronized by the state instance. The default is to expand a clients
    // tabs, so "not present" means "expanded".
    // Only accessed from the UI thread.
    protected final Set<String> collapsedClients;

    // Synchronized by the state instance. The default is to show a client, so
    // "not present" means "shown".
    // Only accessed from the UI thread.
    protected final Set<String> hiddenClients;

    // Synchronized by the state instance. The last user selected client guid.
    // The selectedClient may be invalid or null.
    protected String selectedClient;

    public RemoteTabsExpandableListState(SharedPreferences sharedPrefs) {
        if (null == sharedPrefs) {
            throw new IllegalArgumentException("sharedPrefs must not be null");
        }
        this.sharedPrefs = sharedPrefs;

        this.collapsedClients = getStringSet(PREF_COLLAPSED_CLIENT_GUIDS);
        this.hiddenClients = getStringSet(PREF_HIDDEN_CLIENT_GUIDS);
        this.selectedClient = sharedPrefs.getString(PREF_SELECTED_CLIENT_GUID, null);
    }

    /**
     * Extract a string set from shared preferences.
     * <p>
     * Nota bene: it is not OK to modify the set returned by {@link SharedPreferences#getStringSet(String, Set)}.
     *
     * @param pref to read from.
     * @returns string set; never null.
     */
    protected Set<String> getStringSet(String pref) {
        final Set<String> loaded = PrefUtils.getStringSet(sharedPrefs, pref, null);
        if (loaded != null) {
            return new HashSet<String>(loaded);
        } else {
            return new HashSet<String>();
        }
    }

    /**
     * Update client membership in a set.
     *
     * @param pref
     *            to write updated set to.
     * @param clients
     *            set to update membership in.
     * @param clientGuid
     *            to update membership of.
     * @param isMember
     *            whether the client is a member of the set.
     * @return true if the set of clients was modified.
     */
    protected boolean updateClientMembership(String pref, Set<String> clients, String clientGuid, boolean isMember) {
        final boolean modified;
        if (isMember) {
            modified = clients.add(clientGuid);
        } else {
            modified = clients.remove(clientGuid);
        }

        if (modified) {
            // This starts an asynchronous write. We don't care if we drop the
            // write, and we don't really care if we race between writes, since
            // we will return results from our in-memory cache.
            final Editor editor = sharedPrefs.edit();
            PrefUtils.putStringSet(editor, pref, clients);
            editor.apply();
        }

        return modified;
    }

    /**
     * Mark a client as collapsed.
     *
     * @param clientGuid
     *            to update.
     * @param collapsed
     *            whether the client is collapsed.
     * @return true if the set of collapsed clients was modified.
     */
    protected synchronized boolean setClientCollapsed(String clientGuid, boolean collapsed) {
        return updateClientMembership(PREF_COLLAPSED_CLIENT_GUIDS, collapsedClients, clientGuid, collapsed);
    }

    /**
     * Mark a client as the selected.
     *
     * @param clientGuid
     *            to update.
     */
    protected synchronized void setClientAsSelected(String clientGuid) {
        if (hiddenClients.contains(clientGuid)) {
            selectedClient = null;
        } else {
            selectedClient = clientGuid;
        }

        final Editor editor = sharedPrefs.edit();
        editor.putString(PREF_SELECTED_CLIENT_GUID, selectedClient);
        editor.apply();
    }

    public synchronized boolean isClientCollapsed(String clientGuid) {
        return collapsedClients.contains(clientGuid);
    }

    /**
     * Mark a client as hidden.
     *
     * @param clientGuid
     *            to update.
     * @param hidden
     *            whether the client is hidden.
     * @return true if the set of hidden clients was modified.
     */
    protected synchronized boolean setClientHidden(String clientGuid, boolean hidden) {
        return updateClientMembership(PREF_HIDDEN_CLIENT_GUIDS, hiddenClients, clientGuid, hidden);
    }

    public synchronized boolean isClientHidden(String clientGuid) {
        return hiddenClients.contains(clientGuid);
    }
}
