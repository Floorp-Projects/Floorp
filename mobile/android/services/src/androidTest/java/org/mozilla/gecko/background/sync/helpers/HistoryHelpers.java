/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;

public class HistoryHelpers {

  @SuppressWarnings("unchecked")
  private static JSONArray getVisits1() {
    JSONArray json = new JSONArray();
    JSONObject obj = new JSONObject();
    obj.put("date", 1320087601465600000L);
    obj.put("type", 2L);
    json.add(obj);
    obj = new JSONObject();
    obj.put("date", 1320084970724990000L);
    obj.put("type", 1L);
    json.add(obj);
    obj = new JSONObject();
    obj.put("date", 1319764134412287000L);
    obj.put("type", 1L);
    json.add(obj);
    obj = new JSONObject();
    obj.put("date", 1319681306455594000L);
    obj.put("type", 2L);
    json.add(obj);
    return json;
  }

  @SuppressWarnings("unchecked")
  private static JSONArray getVisits2() {
    JSONArray json = new JSONArray();
    JSONObject obj = new JSONObject();
    obj = new JSONObject();
    obj.put("date", 1319764134412345000L);
    obj.put("type", 4L);
    json.add(obj);
    obj = new JSONObject();
    obj.put("date", 1319681306454321000L);
    obj.put("type", 3L);
    json.add(obj);
    return json;
  }

  public static HistoryRecord createHistory1() {
    HistoryRecord record = new HistoryRecord();
    record.title          = "History 1";
    record.histURI        = "http://history.page1.com";
    record.visits         = getVisits1();
    return record;
  }


  public static HistoryRecord createHistory2() {
    HistoryRecord record = new HistoryRecord();
    record.title          = "History 2";
    record.histURI        = "http://history.page2.com";
    record.visits         = getVisits2();
    return record;
  }

  public static HistoryRecord createHistory3() {
    HistoryRecord record = new HistoryRecord();
    record.title          = "History 3";
    record.histURI        = "http://history.page3.com";
    record.visits         = getVisits2();
    return record;
  }

  public static HistoryRecord createHistory4() {
    HistoryRecord record = new HistoryRecord();
    record.title          = "History 4";
    record.histURI        = "http://history.page4.com";
    record.visits         = getVisits1();
    return record;
  }

  public static HistoryRecord createHistory5() {
    HistoryRecord record = new HistoryRecord();
    record.title          = "History 5";
    record.histURI        = "http://history.page5.com";
    record.visits         = getVisits2();
    return record;
  }

}