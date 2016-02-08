/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

/**
 * Thin container for a subscribe channel response.
 */
public class SubscribeChannelResponse {
    public final String channelID;
    public final String endpoint;

    public SubscribeChannelResponse(String channelID, String endpoint) {
        this.channelID = channelID;
        this.endpoint = endpoint;
    }
}
