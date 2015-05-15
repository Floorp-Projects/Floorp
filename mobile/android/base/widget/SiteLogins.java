package org.mozilla.gecko.widget;

import org.json.JSONArray;
import org.json.JSONObject;

public class SiteLogins {
    private final JSONObject titleObj;
    private final JSONArray logins;

    public SiteLogins(JSONObject titleObj, JSONArray logins) {
        this.logins = logins;
        this.titleObj = titleObj;
    }

    public JSONArray getLogins() {
        return logins;
    }

    public JSONObject getTitle() {
        return titleObj;
    }
}
