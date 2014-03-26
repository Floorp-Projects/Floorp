package org.mozilla.gecko.prompts;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.GeckoAppShell;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.util.Log;

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
    public final boolean showAsActions;
    public final boolean isParent;

    public Intent mIntent;
    public boolean mSelected;
    public Drawable mIcon;

    PromptListItem(JSONObject aObject) {
        label = aObject.isNull("label") ? "" : aObject.optString("label");
        isGroup = aObject.optBoolean("isGroup");
        inGroup = aObject.optBoolean("inGroup");
        disabled = aObject.optBoolean("disabled");
        id = aObject.optInt("id");
        mSelected = aObject.optBoolean("selected");

        JSONObject obj = aObject.optJSONObject("showAsActions");
        if (obj != null) {
            showAsActions = true;
            String uri = obj.isNull("uri") ? "" : obj.optString("uri");
            String type = obj.isNull("type") ? "text/html" : obj.optString("type", "text/html");
            mIntent = GeckoAppShell.getShareIntent(GeckoAppShell.getContext(), uri, type, "");
            isParent = true;
        } else {
            mIntent = null;
            showAsActions = false;
            // Support both "isParent" (backwards compat for older consumers), and "menu" for the new Tabbed prompt ui.
            isParent = aObject.optBoolean("isParent") || aObject.optBoolean("menu");
        }

        BitmapUtils.getDrawable(GeckoAppShell.getContext(), aObject.optString("icon"), new BitmapUtils.BitmapLoader() {
            @Override
            public void onBitmapFound(Drawable d) {
                mIcon = d;
            }
        });
    }

    public void setIntent(Intent i) {
        mIntent = i;
    }

    public Intent getIntent() {
        return mIntent;
    }

    public void setIcon(Drawable icon) {
        mIcon = icon;
    }

    public Drawable getIcon() {
        return mIcon;
    }

    public void setSelected(boolean selected) {
        mSelected = selected;
    }

    public boolean getSelected() {
        return mSelected;
    }

    public PromptListItem(String aLabel) {
        label = aLabel;
        isGroup = false;
        inGroup = false;
        isParent = false;
        disabled = false;
        id = 0;
        showAsActions = false;
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
