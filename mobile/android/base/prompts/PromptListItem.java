package org.mozilla.gecko.prompts;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import android.content.Intent;
import android.graphics.drawable.Drawable;

import java.util.List;
import java.util.ArrayList;

// This class should die and be replaced with normal menu items
public class PromptListItem {
    private static final String LOGTAG = "GeckoPromptListItem";
    public final String label;
    public final boolean isGroup;
    public final boolean inGroup;
    public final boolean disabled;
    public final int id;
    public boolean selected;
    public Intent intent;

    public boolean isParent;
    public Drawable icon;

    PromptListItem(JSONObject aObject) {
        label = aObject.optString("label");
        isGroup = aObject.optBoolean("isGroup");
        inGroup = aObject.optBoolean("inGroup");
        disabled = aObject.optBoolean("disabled");
        id = aObject.optInt("id");
        isParent = aObject.optBoolean("isParent");
        selected = aObject.optBoolean("selected");
    }

    public PromptListItem(String aLabel) {
        label = aLabel;
        isGroup = false;
        inGroup = false;
        disabled = false;
        id = 0;
    }

    static PromptListItem[] getArray(JSONArray items) {
        if (items == null) {
            return new PromptListItem[0];
        }

        int length = items.length();
        List<PromptListItem> list = new ArrayList<PromptListItem>(length);
        for (int i = 0; i < length; i++) {
            try {
                PromptListItem item = new PromptListItem(items.getJSONObject(i));
                list.add(item);
            } catch(Exception ex) { }
        }

        PromptListItem[] arrays = new PromptListItem[length];
        list.toArray(arrays);
        return arrays;
    }
}
