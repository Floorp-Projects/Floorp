/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.text;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Text selection action like "copy", "paste", ..
 */
public class TextAction {
    private static final String LOGTAG = "GeckoTextAction";

    private String id;
    private String label;
    private int order;
    private int floatingOrder;

    private TextAction() {}

    public static List<TextAction> fromEventMessage(JSONObject message) {
        final List<TextAction> actions = new ArrayList<>();

        try {
            final JSONArray array = message.getJSONArray("actions");

            for (int i = 0; i < array.length(); i++) {
                final JSONObject object = array.getJSONObject(i);

                final TextAction action = new TextAction();
                action.id = object.getString("id");
                action.label = object.getString("label");
                action.order = object.getInt("order");
                action.floatingOrder = object.optInt("floatingOrder", i);

                actions.add(action);
            }
        } catch (JSONException e) {
            Log.w(LOGTAG, "Could not parse text actions", e);
        }

        return actions;
    }

    public String getId() {
        return id;
    }

    public String getLabel() {
        return label;
    }

    public int getOrder() {
        return order;
    }

    public int getFloatingOrder() {
        return floatingOrder;
    }
}
