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
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.RelativeLayout;
import android.widget.ListView;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class FormAssistPopup extends ListView implements GeckoEventListener {
    private Context mContext;
    private RelativeLayout.LayoutParams mLayout;

    private int mWidth;
    private int mHeight;

    private Animation mAnimation; 

    private static final String LOGTAG = "FormAssistPopup";

    private static int sMinWidth = 0;
    private static int sRowHeight = 0;
    private static final int POPUP_MIN_WIDTH_IN_DPI = 200;
    private static final int POPUP_ROW_HEIGHT_IN_DPI = 32;

    private static enum PopupType { NONE, AUTOCOMPLETE, VALIDATION };

    // Keep track of the type of popup we're currently showing
    private PopupType mTypeShowing = PopupType.NONE;

    public FormAssistPopup(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mAnimation = AnimationUtils.loadAnimation(context, R.anim.grow_fade_in);
        mAnimation.setDuration(75);

        setFocusable(false);

        setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parentView, View view, int position, long id) {
                if (mTypeShowing.equals(PopupType.AUTOCOMPLETE)) {
                    // Use the value stored with the autocomplete view, not the label text,
                    // since they can be different.
                    TextView textView = (TextView) view;
                    String value = (String) textView.getTag();
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FormAssist:AutoComplete", value));
                    hide();
                }
            }
        });

        GeckoAppShell.registerGeckoEventListener("FormAssist:AutoComplete", this);
        GeckoAppShell.registerGeckoEventListener("FormAssist:ValidationMessage", this);
        GeckoAppShell.registerGeckoEventListener("FormAssist:Hide", this);
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("FormAssist:AutoComplete")) {
                handleAutoCompleteMessage(message);
            } else if (event.equals("FormAssist:ValidationMessage")) {
                handleValidationMessage(message);
            } else if (event.equals("FormAssist:Hide")) {
                handleHideMessage(message);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    private void handleAutoCompleteMessage(JSONObject message) throws JSONException  {
        final JSONArray suggestions = message.getJSONArray("suggestions");
        final JSONArray rect = message.getJSONArray("rect");
        final double zoom = message.getDouble("zoom");
        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                showAutoCompleteSuggestions(suggestions, rect, zoom);
            }
        });
    }

    private void handleValidationMessage(JSONObject message) throws JSONException {
        final String validationMessage = message.getString("validationMessage");
        final JSONArray rect = message.getJSONArray("rect");
        final double zoom = message.getDouble("zoom");
        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                showValidationMessage(validationMessage, rect, zoom);
            }
        });
    }
    
    private void handleHideMessage(JSONObject message) {
        GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
            public void run() {
                hide();
            }
        });
    }

    private void showAutoCompleteSuggestions(JSONArray suggestions, JSONArray rect, double zoom) {
        AutoCompleteListAdapter adapter = new AutoCompleteListAdapter(mContext, R.layout.autocomplete_list_item);
        adapter.populateSuggestionsList(suggestions);
        setAdapter(adapter);

        if (positionAndShowPopup(rect, zoom))
            mTypeShowing = PopupType.AUTOCOMPLETE;
    }

    // TODO: style the validation message popup differently (bug 731654)
    private void showValidationMessage(String validationMessage, JSONArray rect, double zoom) {
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(mContext, R.layout.autocomplete_list_item);
        adapter.add(validationMessage);
        setAdapter(adapter);

        if (positionAndShowPopup(rect, zoom))
            mTypeShowing = PopupType.VALIDATION;
    }

    // Returns true if the popup is successfully shown, false otherwise
    public boolean positionAndShowPopup(JSONArray rect, double zoom) {
        // Don't show the form assist popup when using fullscreen VKB
        InputMethodManager imm =
                (InputMethodManager) GeckoApp.mAppContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm.isFullscreenMode())
            return false;

        if (!isShown()) {
            setVisibility(View.VISIBLE);
            startAnimation(mAnimation);
        }

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
            sMinWidth = (int) (POPUP_MIN_WIDTH_IN_DPI * metrics.density);
            sRowHeight = (int) (POPUP_ROW_HEIGHT_IN_DPI * metrics.density);
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

        listHeight = sRowHeight * getAdapter().getCount();

        // The text box doesnt fit below
        if ((listTop + listHeight) > viewport.height) {
            // Find where the maximum space is, and fit it there
            if ((viewport.height - listTop) > top) {
                // Shrink the height to fit it below the text-box
                listHeight = (int) (viewport.height - listTop);
            } else {
                if (listHeight < top) {
                    // No shrinking needed to fit on top
                    listTop = (top - listHeight);
                } else {
                    // Shrink to available space on top
                    listTop = 0;
                    listHeight = top;
                }
           }
        }

        mLayout = new RelativeLayout.LayoutParams(listWidth, listHeight);
        mLayout.setMargins(listLeft, listTop, 0, 0);
        setLayoutParams(mLayout);
        requestLayout();

        return true;
    }

    public void hide() {
        if (isShown()) {
            setVisibility(View.GONE);
            mTypeShowing = PopupType.NONE;
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FormAssist:Hidden", null));
        }
    }

    private class AutoCompleteListAdapter extends ArrayAdapter<Pair<String, String>> {
        private LayoutInflater mInflater;
        private int mTextViewResourceId;

        public AutoCompleteListAdapter(Context context, int textViewResourceId) {
            super(context, textViewResourceId);

            mInflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            mTextViewResourceId = textViewResourceId;
        }

        // This method takes an array of autocomplete suggestions with label/value properties
        // and adds label/value Pair objects to the array that backs the adapter.
        public void populateSuggestionsList(JSONArray suggestions) {
            try {
                for (int i = 0; i < suggestions.length(); i++) {
                    JSONObject suggestion = (JSONObject) suggestions.get(i);
                    String label = suggestion.getString("label");
                    String value = suggestion.getString("value");
                    add(new Pair<String, String>(label, value));
                }
            } catch (JSONException e) {
                Log.e(LOGTAG, "JSONException: " + e);
            }
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null)
                convertView = mInflater.inflate(mTextViewResourceId, null);

            Pair<String, String> item = getItem(position);
            TextView itemView = (TextView) convertView;

            // Set the text with the suggestion label
            itemView.setText(item.first);

            // Set a tag with the suggestion value
            itemView.setTag(item.second);

            return convertView;
        }
    }
}
