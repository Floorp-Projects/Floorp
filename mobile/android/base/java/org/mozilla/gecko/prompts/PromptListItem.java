package org.mozilla.gecko.prompts;

import org.mozilla.gecko.IntentHelper;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.ThumbnailHelper;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ResourceDrawableUtils;
import org.mozilla.gecko.widget.GeckoActionProvider;

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

    PromptListItem(GeckoBundle aObject) {
        final Context context = GeckoAppShell.getApplicationContext();
        label = aObject.getString("label", "");
        isGroup = aObject.getBoolean("isGroup");
        inGroup = aObject.getBoolean("inGroup");
        disabled = aObject.getBoolean("disabled");
        id = aObject.getInt("id");
        mSelected = aObject.getBoolean("selected");

        GeckoBundle obj = aObject.getBundle("showAsActions");
        if (obj != null) {
            showAsActions = true;
            String uri = obj.getString("uri", "");
            String type = obj.getString("type", GeckoActionProvider.DEFAULT_MIME_TYPE);

            mIntent = IntentHelper.getShareIntent(context, uri, type, "");
            isParent = true;
        } else {
            mIntent = null;
            showAsActions = false;
            // Support both "isParent" (backwards compat for older consumers), and "menu"
            // for the new Tabbed prompt ui.
            isParent = aObject.getBoolean("isParent") || aObject.getBoolean("menu");
        }

        final String iconStr = aObject.getString("icon");
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

    static PromptListItem[] getArray(GeckoBundle[] items) {
        if (items == null) {
            return new PromptListItem[0];
        }

        int length = items.length;
        List<PromptListItem> list = new ArrayList<>(length);
        for (int i = 0; i < length; i++) {
            PromptListItem item = new PromptListItem(items[i]);
            list.add(item);
        }

        return list.toArray(new PromptListItem[length]);
    }
}
