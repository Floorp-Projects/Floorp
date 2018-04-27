/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

import org.json.JSONObject;

/**
 * Provide access to the tab API.
 */
public final class Tab extends Actor {
    public final String title;
    public final String url;
    public final long outerWindowID;
    private final JSONObject mTab;

    /* package */ Tab(final RDPConnection connection, final JSONObject tab) {
        super(connection, tab);
        title = tab.optString("title", null);
        url = tab.optString("url", null);
        outerWindowID = tab.optLong("outerWindowID", -1);
        mTab = tab;
    }

    /**
     * Attach to the server tab.
     */
    public void attach() {
        sendPacket("{\"type\":\"attach\"}", "type");
    }

    /**
     * Detach from the server tab.
     */
    public void detach() {
        sendPacket("{\"type\":\"detach\"}", "type");
    }

    /**
     * Get the console object for access to the webconsole API.
     *
     * @return Console object.
     */
    public Console getConsole() {
        final String name = mTab.optString("consoleActor", null);
        final Actor console = connection.getActor(name);
        return (console != null) ? (Console) console : new Console(connection, name);
    }
}
