package org.mozilla.gecko.widget;

import org.json.JSONArray;
import org.json.JSONObject;

public class SiteLogins {
    private final JSONArray logins;

    public SiteLogins(JSONArray logins) {
        this.logins = logins;
    }

    public JSONArray getLogins() {
        return logins;
    }
}
