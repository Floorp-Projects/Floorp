/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import java.util.ArrayList;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ResourceDrawableUtils;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.TextView;

public class IconGridInput extends PromptInput implements OnItemClickListener {
    public static final String INPUT_TYPE = "icongrid";
    public static final String LOGTAG = "GeckoIconGridInput";

    private ArrayAdapter<IconGridItem> mAdapter; // An adapter holding a list of items to show in the grid

    private static int mColumnWidth = -1;  // The maximum width of columns
    private static int mMaxColumns = -1;  // The maximum number of columns to show
    private static int mIconSize = -1;    // Size of icons in the grid
    private int mSelected;                // Current selection
    private final JSONArray mArray;

    public IconGridInput(JSONObject obj) {
        super(obj);
        mArray = obj.optJSONArray("items");
    }

    @Override
    public View getView(Context context) throws UnsupportedOperationException {
        if (mColumnWidth < 0) {
            // getColumnWidth isn't available on pre-ICS, so we pull it out and assign it here
            mColumnWidth = context.getResources().getDimensionPixelSize(R.dimen.icongrid_columnwidth);
        }

        if (mIconSize < 0) {
            mIconSize = GeckoAppShell.getPreferredIconSize();
        }

        if (mMaxColumns < 0) {
            mMaxColumns = context.getResources().getInteger(R.integer.max_icon_grid_columns);
        }

        // TODO: Dynamically handle size changes
        final WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        final Display display = wm.getDefaultDisplay();
        final int screenWidth = display.getWidth();
        int maxColumns = Math.min(mMaxColumns, screenWidth / mColumnWidth);

        final GridView view = (GridView) LayoutInflater.from(context).inflate(R.layout.icon_grid, null, false);
        view.setColumnWidth(mColumnWidth);

        final ArrayList<IconGridItem> items = new ArrayList<IconGridItem>(mArray.length());
        for (int i = 0; i < mArray.length(); i++) {
            IconGridItem item = new IconGridItem(context, mArray.optJSONObject(i));
            items.add(item);
            if (item.selected) {
                mSelected = i;
            }
        }

        view.setNumColumns(Math.min(items.size(), maxColumns));
        view.setOnItemClickListener(this);
        // Despite what the docs say, setItemChecked was not moved into the AbsListView class until sometime between
        // Android 2.3.7 and Android 4.0.3. For other versions the item won't be visually highlighted, BUT we really only
        // mSelected will still be set so that we default to its behavior.
        if (mSelected > -1) {
            view.setItemChecked(mSelected, true);
        }

        mAdapter = new IconGridAdapter(context, -1, items);
        view.setAdapter(mAdapter);
        mView = view;
        return mView;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mSelected = position;
        notifyListeners(Integer.toString(position));
    }

    @Override
    public Object getValue() {
        return mSelected;
    }

    @Override
    public boolean getScrollable() {
        return true;
    }

    private class IconGridAdapter extends ArrayAdapter<IconGridItem> {
        public IconGridAdapter(Context context, int resource, List<IconGridItem> items) {
            super(context, resource, items);
        }

        @Override
        public View getView(int position, View convert, ViewGroup parent) {
            final Context context = parent.getContext();
            if (convert == null) {
                convert = LayoutInflater.from(context).inflate(R.layout.icon_grid_item, parent, false);
            }
            bindView(convert, context, position);
            return convert;
        }

        private void bindView(View v, Context c, int position) {
            final IconGridItem item = getItem(position);
            final TextView text1 = (TextView) v.findViewById(android.R.id.text1);
            text1.setText(item.label);

            final TextView text2 = (TextView) v.findViewById(android.R.id.text2);
            if (TextUtils.isEmpty(item.description)) {
                text2.setVisibility(View.GONE);
            } else {
                text2.setVisibility(View.VISIBLE);
                text2.setText(item.description);
            }

            final ImageView icon = (ImageView) v.findViewById(R.id.icon);
            icon.setImageDrawable(item.icon);
            ViewGroup.LayoutParams lp = icon.getLayoutParams();
            lp.width = lp.height = mIconSize;
        }
    }

    private class IconGridItem {
        final String label;
        final String description;
        final boolean selected;
        Drawable icon;

        public IconGridItem(final Context context, final JSONObject obj) {
            label = obj.optString("name");
            final String iconUrl = obj.optString("iconUri");
            description = obj.optString("description");
            selected = obj.optBoolean("selected");

            ResourceDrawableUtils.getDrawable(context, iconUrl, new ResourceDrawableUtils.BitmapLoader() {
                @Override
                public void onBitmapFound(Drawable d) {
                    icon = d;
                    if (mAdapter != null) {
                        mAdapter.notifyDataSetChanged();
                    }
                }
            });
        }
    }
}
