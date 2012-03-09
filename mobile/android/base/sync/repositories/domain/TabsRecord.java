/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;

import java.util.ArrayList;

import org.json.simple.JSONObject;
import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.Utils;

/**
 * Represents a client's collection of tabs.
 *
 * @author rnewman
 *
 */
public class TabsRecord extends Record {

  // Immutable.
  public static class Tab {
    public final String    title;
    public final String    icon;
    public final JSONArray history;
    public final long      lastUsed;

    public Tab(String title, String icon, JSONArray history, long lastUsed) {
      this.title    = title;
      this.icon     = icon;
      this.history  = history;
      this.lastUsed = lastUsed;
    }

    public static Tab fromJSONObject(JSONObject o) throws NonArrayJSONException {
      ExtendedJSONObject obj = new ExtendedJSONObject(o);
      String title      = obj.getString("title");
      String icon       = obj.getString("icon");
      JSONArray history = obj.getArray("urlHistory");
      long lastUsed     = obj.getLong("lastUsed");
      return new Tab(title, icon, history, lastUsed);
    }

    @SuppressWarnings("unchecked")
    public JSONObject toJSONObject() {
      JSONObject o = new JSONObject();
      o.put("title", title);
      o.put("icon", icon);
      o.put("urlHistory", history);
      o.put("lastUsed", lastUsed);
      return o;
    }
  }

  private static final String LOG_TAG = "TabsRecord";

  public static final String COLLECTION_NAME = "tabs";

  public TabsRecord(String guid, String collection, long lastModified, boolean deleted) {
    super(guid, collection, lastModified, deleted);
  }
  public TabsRecord(String guid, String collection, long lastModified) {
    super(guid, collection, lastModified, false);
  }
  public TabsRecord(String guid, String collection) {
    super(guid, collection, 0, false);
  }
  public TabsRecord(String guid) {
    super(guid, COLLECTION_NAME, 0, false);
  }
  public TabsRecord() {
    super(Utils.generateGuid(), COLLECTION_NAME, 0, false);
  }

  public String clientName;
  public ArrayList<Tab> tabs;

  @Override
  public void initFromPayload(ExtendedJSONObject payload) {
    clientName = (String) payload.get("clientName");
    try {
      tabs = tabsFrom(payload.getArray("tabs"));
    } catch (NonArrayJSONException e) {
      // Oh well.
      tabs = new ArrayList<Tab>();
    }
  }

  @SuppressWarnings("unchecked")
  protected static JSONArray tabsToJSON(ArrayList<Tab> tabs) {
    JSONArray out = new JSONArray();
    for (Tab tab : tabs) {
      out.add(tab.toJSONObject());
    }
    return out;
  }

  protected static ArrayList<Tab> tabsFrom(JSONArray in) {
    ArrayList<Tab> tabs = new ArrayList<Tab>(in.size());
    for (Object o : in) {
      if (o instanceof JSONObject) {
        try {
          tabs.add(Tab.fromJSONObject((JSONObject) o));
        } catch (NonArrayJSONException e) {
          Logger.warn(LOG_TAG, "urlHistory is not an array for this tab.", e);
        }
      }
    }
    return tabs;
  }

  @Override
  public void populatePayload(ExtendedJSONObject payload) {
    putPayload(payload, "id", this.guid);
    putPayload(payload, "clientName", this.clientName);
    payload.put("tabs", tabsToJSON(this.tabs));
  }

  @Override
  public Record copyWithIDs(String guid, long androidID) {
    TabsRecord out = new TabsRecord(guid, this.collection, this.lastModified, this.deleted);
    out.androidID = androidID;
    out.sortIndex = this.sortIndex;

    out.clientName = this.clientName;
    out.tabs = new ArrayList<Tab>(this.tabs);

    return out;
  }
}
