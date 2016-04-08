/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.CheckedTextView;
import android.widget.ListView;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;

public class BasicColorPicker extends ListView {
    private final static String LOGTAG = "GeckoBasicColorPicker";
    private final static List<Integer> DEFAULT_COLORS = Arrays.asList(Color.rgb(215, 57, 32),
                                                                      Color.rgb(255, 134, 5),
                                                                      Color.rgb(255, 203, 19),
                                                                      Color.rgb(95, 173, 71),
                                                                      Color.rgb(84, 201, 168),
                                                                      Color.rgb(33, 161, 222),
                                                                      Color.rgb(16, 36, 87),
                                                                      Color.rgb(91, 32, 103),
                                                                      Color.rgb(212, 221, 228),
                                                                      Color.BLACK);

    private static Drawable mCheckDrawable;
    int mSelected;
    final ColorPickerListAdapter mAdapter;

    public BasicColorPicker(Context context) {
        this(context, null);
    }

    public BasicColorPicker(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public BasicColorPicker(Context context, AttributeSet attrs, int style) {
        this(context, attrs, style, DEFAULT_COLORS);
    }

    public BasicColorPicker(Context context, AttributeSet attrs, int style, List<Integer> colors) {
        super(context, attrs, style);
        mAdapter = new ColorPickerListAdapter(context, new ArrayList<Integer>(colors));
        setAdapter(mAdapter);

        setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                mSelected = position;
                mAdapter.notifyDataSetChanged();
            }
        });
    }

    public int getColor() {
        return mAdapter.getItem(mSelected);
    }

    public void setColor(int color) {
        if (!DEFAULT_COLORS.contains(color)) {
            mSelected = mAdapter.getCount();
            mAdapter.add(color);
        } else {
            mSelected = DEFAULT_COLORS.indexOf(color);
        }

        setSelection(mSelected);
        mAdapter.notifyDataSetChanged();
    }

    Drawable getCheckDrawable() {
        if (mCheckDrawable == null) {
            Resources res = getContext().getResources();

            TypedValue typedValue = new TypedValue();
            getContext().getTheme().resolveAttribute(android.R.attr.listPreferredItemHeight, typedValue, true);
            DisplayMetrics metrics = new android.util.DisplayMetrics();
            ((WindowManager)getContext().getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getMetrics(metrics);
            int height = (int) typedValue.getDimension(metrics);

            Drawable background = res.getDrawable(R.drawable.color_picker_row_bg);
            Rect r = new Rect();
            background.getPadding(r);
            height -= r.top + r.bottom;

            mCheckDrawable = res.getDrawable(R.drawable.color_picker_checkmark);
            mCheckDrawable.setBounds(0, 0, height, height);
        }

        return mCheckDrawable;
    }

   private class ColorPickerListAdapter extends ArrayAdapter<Integer> {
        private final List<Integer> mColors;

        public ColorPickerListAdapter(Context context, List<Integer> colors) {
            super(context, R.layout.color_picker_row, colors);
            mColors = colors;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View v = super.getView(position, convertView, parent);

            Drawable d = v.getBackground();
            d.setColorFilter(getItem(position), PorterDuff.Mode.MULTIPLY);
            v.setBackgroundDrawable(d);

            Drawable check = null;
            CheckedTextView checked = ((CheckedTextView) v);
            if (mSelected == position) {
                check = getCheckDrawable();
            }

            checked.setCompoundDrawables(check, null, null, null);
            checked.setText("");

            return v;
        }
    }
}
