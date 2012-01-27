/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Margaret Leibovic <margaret.leibovic@gmail.com>
 *   Sriram Ramasubramanian <sriram@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.FloatSize;

import android.content.Context;
import android.util.Log;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.RelativeLayout;
import android.widget.ListView;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONException;

public class AutoCompletePopup extends ListView {
    private Context mContext;
    private RelativeLayout.LayoutParams mLayout;

    private int mWidth;
    private int mHeight;

    private Animation mAnimation; 

    private static final String LOGTAG = "AutoCompletePopup";

    private static int sMinWidth = 0;
    private static final int AUTOCOMPLETE_MIN_WIDTH_IN_DPI = 200;

    public AutoCompletePopup(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mAnimation = AnimationUtils.loadAnimation(context, R.anim.grow_fade_in);
        mAnimation.setDuration(75);

        setFocusable(false);

        setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parentView, View view, int position, long id) {
                String value = ((TextView) view).getText().toString();
                GeckoAppShell.sendEventToGecko(new GeckoEvent("FormAssist:AutoComplete", value));
                hide();
            }
        });
    }

    public void show(JSONArray suggestions, JSONArray rect, double zoom) {
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(mContext, R.layout.autocomplete_list_item);
        for (int i = 0; i < suggestions.length(); i++) {
            try {
                adapter.add(suggestions.get(i).toString());
            } catch (JSONException e) {
                Log.i(LOGTAG, "JSONException: " + e);
            }
        }

        setAdapter(adapter);

        if (isShown())
            return;

        setVisibility(View.VISIBLE);
        startAnimation(mAnimation);

        if (mLayout == null) {
            mLayout = (RelativeLayout.LayoutParams) getLayoutParams();
            mWidth = mLayout.width;
            mHeight = mLayout.height;
        }

        int left = 0;
        int top = 0; 
        int width = 0;
        int height = 0;

        try {
            left = (int) (rect.getDouble(0) * zoom);
            top = (int) (rect.getDouble(1) * zoom);
            width = (int) (rect.getDouble(2) * zoom);
            height = (int) (rect.getDouble(3) * zoom);
        } catch (JSONException e) { } 

        int listWidth = mWidth;
        int listHeight = mHeight;
        int listLeft = left < 0 ? 0 : left;
        int listTop = top + height;

        FloatSize viewport = GeckoApp.mAppContext.getLayerController().getViewportSize();

        // Late initializing variable to allow DisplayMetrics not to be null and avoid NPE
        if (sMinWidth == 0) {
            DisplayMetrics metrics = new DisplayMetrics();
            GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);
            sMinWidth = (int) (AUTOCOMPLETE_MIN_WIDTH_IN_DPI * metrics.density);
        }

        // If the textbox is smaller than the screen-width,
        // shrink the list's width
        if ((left + width) < viewport.width) 
            listWidth = left < 0 ? left + width : width;

        // listWidth can be negative if it is a constant - FILL_PARENT or MATCH_PARENT
        if (listWidth >= 0 && listWidth < sMinWidth) {
            listWidth = sMinWidth;

            if ((listLeft + listWidth) > viewport.width)
                listLeft = (int) (viewport.width - listWidth);
        }

        // If the list is extending outside of the viewport
        // try moving above
        if (((listTop + listHeight) > viewport.height) && (listHeight <= top))
            listTop = (top - listHeight);

        mLayout = new RelativeLayout.LayoutParams(listWidth, listHeight);
        mLayout.setMargins(listLeft, listTop, 0, 0);
        setLayoutParams(mLayout);
        requestLayout();
    }

    public void hide() {
        if (isShown()) {
            setVisibility(View.GONE);
            GeckoAppShell.sendEventToGecko(new GeckoEvent("FormAssist:Closed", null));
        }
    }
}
