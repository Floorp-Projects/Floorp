/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.text;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.util.GeckoBundle;

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

    public static List<TextAction> fromEventMessage(final GeckoBundle message) {
        final List<TextAction> actions = new ArrayList<>();
        final GeckoBundle[] array = message.getBundleArray("actions");

        for (int i = 0; i < array.length; i++) {
            final GeckoBundle object = array[i];
            final TextAction action = new TextAction();

            action.id = object.getString("id");
            action.label = object.getString("label");
            action.order = object.getInt("order");
            action.floatingOrder = object.getInt("floatingOrder", i);

            actions.add(action);
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
