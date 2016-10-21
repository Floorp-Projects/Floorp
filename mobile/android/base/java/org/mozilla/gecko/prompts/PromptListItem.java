package org.mozilla.gecko.prompts;

import org.json.JSONException;
import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.ThumbnailHelper;
import org.mozilla.gecko.util.ResourceDrawableUtils;
import org.mozilla.gecko.widget.GeckoActionProvider;

import org.json.JSONArray;
import org.json.JSONObject;

import android.content.Context;
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
    public final boolean showAsActions;
    public final boolean isParent;

    public Intent mIntent;
    public boolean mSelected;
    public Drawable mIcon;

    PromptListItem(JSONObject aObject) {
        Context context = GeckoAppShell.getContext();
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
            String type = obj.isNull("type") ? GeckoActionProvider.DEFAULT_MIME_TYPE :
                                               obj.optString("type", GeckoActionProvider.DEFAULT_MIME_TYPE);

            mIntent = IntentHelper.getShareIntent(context, uri, type, "");
            isParent = true;
        } else {
            mIntent = null;
            showAsActions = false;
            // Support both "isParent" (backwards compat for older consumers), and "menu" for the new Tabbed prompt ui.
            isParent = aObject.optBoolean("isParent") || aObject.optBoolean("menu");
        }

        final String iconStr = aObject.optString("icon");
        if (iconStr != null) {
            final ResourceDrawableUtils.BitmapLoader loader = new ResourceDrawableUtils.BitmapLoader() {
                    @Override
                    public void onBitmapFound(Drawable d) {
                        mIcon = d;
                    }
                };

            if (iconStr.startsWith("thumbnail:")) {
                final int id = Integer.parseInt(iconStr.substring(10), 10);
                ThumbnailHelper.getInstance().getAndProcessThumbnailFor(id, loader);
            } else {
                ResourceDrawableUtils.getDrawable(context, iconStr, loader);
            }
        }
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
        List<PromptListItem> list = new ArrayList<>(length);
        for (int i = 0; i < length; i++) {
            try {
                PromptListItem item = new PromptListItem(items.getJSONObject(i));
                list.add(item);
            } catch (JSONException ex) { }
        }

        return list.toArray(new PromptListItem[length]);
    }
}
