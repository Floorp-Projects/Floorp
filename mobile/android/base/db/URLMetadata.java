/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.database.Cursor;

import org.json.JSONObject;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

public interface URLMetadata {
    public Map<String, Object> fromJSON(JSONObject obj);
    public Map<String, Map<String, Object>> getForURLs(final ContentResolver cr,
                                                       final List<String> urls,
                                                       final List<String> columns);
    public void save(final ContentResolver cr, final String url, final Map<String, Object> data);
}
