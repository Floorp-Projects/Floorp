/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.search;

import org.junit.Test;
import static org.junit.Assert.assertEquals;
import org.junit.runner.RunWith;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.search.SearchEngineManager;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.ArrayList;


/**
 * Unit tests for date utilities.
 */

@RunWith(RobolectricTestRunner.class)
public class TestSearchEngineManager {
    private static String listJSON = "" +
            "{" +
              "'default': {" +
                "'searchDefault': 'Google'" +
              "}," +
              "'regionOverrides': {" +
                "'US': {" +
                  "'google-b-d': 'google-b-1-d'" +
                "}" +
              "}," +
              "'locales': {" +
                "'en-US': {" +
                  "'default': {" +
                    "'visibleDefaultEngines': [" +
                      "'google-b-d', 'amazondotcom', 'bing', 'ddg', 'ebay', 'twitter', 'wikipedia'" +
                    "]" +
                  "}" +
                "}" +
              "}" +
            "}".replace("'", "\"");

    @Test
    public void testGetDefaultEngineNameFromJSON() throws Exception {
        final JSONObject json = new JSONObject(listJSON);
        final String defaultEngine = SearchEngineManager.getDefaultEngineNameFromJSON("US", json);
        assertEquals("Incorrect engine", "Google", defaultEngine);
    }

    @Test
    public void testGetEngineListFromJSON() throws Exception {
        ArrayList<String> correctEngineList = new ArrayList<String>();
        correctEngineList.add("google-b-1-d");
        correctEngineList.add("amazondotcom");
        correctEngineList.add("bing");
        correctEngineList.add("ddg");
        correctEngineList.add("ebay");
        correctEngineList.add("twitter");
        correctEngineList.add("wikipedia");
        final JSONObject json = new JSONObject(listJSON);
        final ArrayList<String> engines = SearchEngineManager.getEngineListFromJSON("US", json);
        assertEquals("Incorrect engine list", correctEngineList, engines);
    }
}
