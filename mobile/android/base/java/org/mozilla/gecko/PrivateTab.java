/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;

import android.support.v4.content.ContextCompat;

import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserDB;

public class PrivateTab extends Tab {
    public PrivateTab(Context context, int id, String url, boolean external, int parentId, String title) {
        super(context, id, url, external, parentId, title);
    }

    @Override
    protected void saveThumbnailToDB(final BrowserDB db) {}

    @Override
    public void setMetadata(JSONObject metadata) {}

    @Override
    public boolean isPrivate() {
        return true;
    }
}
