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
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class FormAssistPopup extends RelativeLayout implements GeckoEventListener {
    private Context mContext;
    private Animation mAnimation; 

    private ListView mAutoCompleteList;
    private RelativeLayout mValidationMessage;
    private TextView mValidationMessageText;
    private ImageView mValidationMessageArrow;
    private ImageView mValidationMessageArrowInverted;

    private static int sAutoCompleteMinWidth = 0;
    private static int sAutoCompleteRowHeight = 0;
    private static int sValidationMessageHeight = 0;
    private static int sValidationTextMarginTop = 0;
    private static RelativeLayout.LayoutParams sValidationTextLayoutNormal;
    private static RelativeLayout.LayoutParams sValidationTextLayoutInverted;

    // Minimum popup width for autocomplete messages
    private static final int AUTOCOMPLETE_MIN_WIDTH_IN_DPI = 200;

    // Height of the autocomplete_list_item TextView
    private static final int AUTOCOMPLETE_ROW_HEIGHT_IN_DPI = 32;

    // Height of the validation_message_text TextView
    private static final int VALIDATION_MESSAGE_HEIGHT_IN_DPI = 50;

    // Top margin for the validation_message_text TextView
    private static final int VALIDATION_MESSAGE_MARGIN_TOP_IN_DPI = 6;

    private static final String LOGTAG = "FormAssistPopup";

    public FormAssistPopup(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mAnimation = AnimationUtils.loadAnimation(context, R.anim.grow_fade_in);
        mAnimation.setDuration(75);

        setFocusable(false);

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
        if (mAutoCompleteList == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            mAutoCompleteList = (ListView) inflater.inflate(R.layout.autocomplete_list, null);

            mAutoCompleteList.setOnItemClickListener(new OnItemClickListener() {
                public void onItemClick(AdapterView<?> parentView, View view, int position, long id) {
                    // Use the value stored with the autocomplete view, not the label text,
                    // since they can be different.
                    TextView textView = (TextView) view;
                    String value = (String) textView.getTag();
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FormAssist:AutoComplete", value));
                    hide();
                }
            });

            addView(mAutoCompleteList);
        }
        
        AutoCompleteListAdapter adapter = new AutoCompleteListAdapter(mContext, R.layout.autocomplete_list_item);
        adapter.populateSuggestionsList(suggestions);
        mAutoCompleteList.setAdapter(adapter);

        positionAndShowPopup(rect, zoom, true);
    }

    private void showValidationMessage(String validationMessage, JSONArray rect, double zoom) {
        if (mValidationMessage == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            mValidationMessage = (RelativeLayout) inflater.inflate(R.layout.validation_message, null);

            addView(mValidationMessage);
            mValidationMessageText = (TextView) mValidationMessage.findViewById(R.id.validation_message_text);

            DisplayMetrics metrics = new DisplayMetrics();
            GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);
            sValidationTextMarginTop = (int) (VALIDATION_MESSAGE_MARGIN_TOP_IN_DPI * metrics.density);

            sValidationTextLayoutNormal = new RelativeLayout.LayoutParams(mValidationMessageText.getLayoutParams());
            sValidationTextLayoutNormal.setMargins(0, sValidationTextMarginTop, 0, 0);

            sValidationTextLayoutInverted = new RelativeLayout.LayoutParams(sValidationTextLayoutNormal);
            sValidationTextLayoutInverted.setMargins(0, 0, 0, 0);

            mValidationMessageArrow = (ImageView) mValidationMessage.findViewById(R.id.validation_message_arrow);
            mValidationMessageArrowInverted = (ImageView) mValidationMessage.findViewById(R.id.validation_message_arrow_inverted);
        }

        mValidationMessageText.setText(validationMessage);

        // We need to set the text as selected for the marquee text to work.
        mValidationMessageText.setSelected(true);

        positionAndShowPopup(rect, zoom, false);
    }

    // Returns true if the popup is successfully shown, false otherwise
    private boolean positionAndShowPopup(JSONArray rect, double zoom, boolean isAutoComplete) {
        // Don't show the form assist popup when using fullscreen VKB
        InputMethodManager imm =
                (InputMethodManager) GeckoApp.mAppContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm.isFullscreenMode())
            return false;

        if (!isShown()) {
            setVisibility(VISIBLE);
            startAnimation(mAnimation);
        }

        // Hide/show the appropriate popup contents
        if (mAutoCompleteList != null)
            mAutoCompleteList.setVisibility(isAutoComplete ? VISIBLE : GONE);
        if (mValidationMessage != null)
            mValidationMessage.setVisibility(isAutoComplete ? GONE : VISIBLE);

        // Initialize static variables based on DisplayMetrics. We delay this to
        // make sure DisplayMetrics isn't null to avoid an NPE.
        if (sAutoCompleteMinWidth == 0) {
            DisplayMetrics metrics = new DisplayMetrics();
            GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);
            sAutoCompleteMinWidth = (int) (AUTOCOMPLETE_MIN_WIDTH_IN_DPI * metrics.density);
            sAutoCompleteRowHeight = (int) (AUTOCOMPLETE_ROW_HEIGHT_IN_DPI * metrics.density);
            sValidationMessageHeight = (int) (VALIDATION_MESSAGE_HEIGHT_IN_DPI * metrics.density);
        }

        // These values correspond to the input box for which we want to
        // display the FormAssistPopup.
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

        int popupWidth = RelativeLayout.LayoutParams.FILL_PARENT;
        int popupLeft = left < 0 ? 0 : left;

        FloatSize viewport = GeckoApp.mAppContext.getLayerController().getViewportSize();

        // For autocomplete suggestions, if the input is smaller than the screen-width,
        // shrink the popup's width. Otherwise, keep it as FILL_PARENT.
        if (isAutoComplete && (left + width) < viewport.width) {
            popupWidth = left < 0 ? left + width : width;

            // Ensure the popup has a minimum width.
            if (popupWidth < sAutoCompleteMinWidth) {
                popupWidth = sAutoCompleteMinWidth;

                // Move the popup to the left if there isn't enough room for it.
                if ((popupLeft + popupWidth) > viewport.width)
                    popupLeft = (int) (viewport.width - popupWidth);
            }
        }

        int popupHeight;
        if (isAutoComplete)
            popupHeight = sAutoCompleteRowHeight * mAutoCompleteList.getAdapter().getCount();
        else
            popupHeight = sValidationMessageHeight;

        int popupTop = top + height;

        if (!isAutoComplete) {
            mValidationMessageText.setLayoutParams(sValidationTextLayoutNormal);
            mValidationMessageArrow.setVisibility(VISIBLE);
            mValidationMessageArrowInverted.setVisibility(GONE);
        }

        // If the popup doesn't fit below the input box, shrink its height, or
        // see if we can place it above the input instead.
        if ((popupTop + popupHeight) > viewport.height) {
            // Find where the maximum space is, and put the popup there.
            if ((viewport.height - popupTop) > top) {
                // Shrink the height to fit it below the input box.
                popupHeight = (int) (viewport.height - popupTop);
            } else {
                if (popupHeight < top) {
                    // No shrinking needed to fit on top.
                    popupTop = (top - popupHeight);
                } else {
                    // Shrink to available space on top.
                    popupTop = 0;
                    popupHeight = top;
                }

                if (!isAutoComplete) {
                    mValidationMessageText.setLayoutParams(sValidationTextLayoutInverted);
                    mValidationMessageArrow.setVisibility(GONE);
                    mValidationMessageArrowInverted.setVisibility(VISIBLE);
                }
           }
        }

        RelativeLayout.LayoutParams layoutParams =
                new RelativeLayout.LayoutParams(popupWidth, popupHeight);
        layoutParams.setMargins(popupLeft, popupTop, 0, 0);
        setLayoutParams(layoutParams);
        requestLayout();

        return true;
    }

    public void hide() {
        if (isShown()) {
            setVisibility(GONE);
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
                    JSONObject suggestion = suggestions.getJSONObject(i);
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
