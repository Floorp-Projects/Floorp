/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.rdp;

/**
 * Provide access to the memory API.
 */
public final class Memory extends Actor {
    /* package */ Memory(final RDPConnection connection, final String name) {
        super(connection, name);
    }

    public void forceCycleCollection() {
        sendPacket("{\"type\":\"forceCycleCollection\"}", JSON_PARSER).get();
    }

    public void forceGarbageCollection() {
        sendPacket("{\"type\":\"forceGarbageCollection\"}", JSON_PARSER).get();
    }
}
