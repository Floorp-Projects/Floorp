/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.SwipeDismissListViewTouchListener;
import org.mozilla.gecko.widget.SwipeDismissListViewTouchListener.OnDismissCallback;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.PointF;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;
import android.widget.TextView;

import java.util.Arrays;
import java.util.Collection;

public class FormAssistPopup extends RelativeLayout implements BundleEventListener {
    private final Context mContext;
    private final Animation mAnimation;

    private ListView mAutoCompleteList;
    private RelativeLayout mValidationMessage;
    private TextView mValidationMessageText;
    private ImageView mValidationMessageArrow;
    private ImageView mValidationMessageArrowInverted;

    private double mX;
    private double mY;
    private double mW;
    private double mH;

    private enum PopupType {
        AUTOCOMPLETE,
        VALIDATIONMESSAGE;
    }
    private PopupType mPopupType;

    private static final int MAX_VISIBLE_ROWS = 5;

    private static int sAutoCompleteMinWidth;
    private static int sAutoCompleteRowHeight;
    private static int sValidationMessageHeight;
    private static int sValidationTextMarginTop;
    private static LayoutParams sValidationTextLayoutNormal;
    private static LayoutParams sValidationTextLayoutInverted;

    private static final String LOGTAG = "GeckoFormAssistPopup";

    // The blocklist is so short that ArrayList is probably cheaper than HashSet.
    private static final Collection<String> sInputMethodBlocklist = Arrays.asList(
                                            InputMethods.METHOD_GOOGLE_JAPANESE_INPUT, // bug 775850
                                            InputMethods.METHOD_OPENWNN_PLUS,          // bug 768108
                                            InputMethods.METHOD_SIMEJI,                // bug 768108
                                            InputMethods.METHOD_SWYPE,                 // bug 755909
                                            InputMethods.METHOD_SWYPE_BETA            // bug 755909
                                            );

    public FormAssistPopup(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mAnimation = AnimationUtils.loadAnimation(context, R.anim.grow_fade_in);
        mAnimation.setDuration(75);

        setFocusable(false);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        GeckoApp.getEventDispatcher().registerUiThreadListener(this,
            "FormAssist:AutoCompleteResult",
            "FormAssist:ValidationMessage",
            "FormAssist:Hide");
    }

    void destroy() {
    }

    @Override
    public void onDetachedFromWindow() {
        GeckoApp.getEventDispatcher().unregisterUiThreadListener(this,
            "FormAssist:AutoCompleteResult",
            "FormAssist:ValidationMessage",
            "FormAssist:Hide");

        super.onDetachedFromWindow();
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("FormAssist:AutoCompleteResult".equals(event)) {
            showAutoCompleteSuggestions(message.getBundleArray("suggestions"),
                                        message.getBundle("rect"),
                                        message.getBoolean("isEmpty"));

        } else if ("FormAssist:ValidationMessage".equals(event)) {
            showValidationMessage(message.getString("validationMessage"),
                                  message.getBundle("rect"));

        } else if ("FormAssist:Hide".equals(event)) {
            hide();
        }
    }

    private void showAutoCompleteSuggestions(final GeckoBundle[] suggestions,
                                             final GeckoBundle rect,
                                             final boolean isEmpty) {
        final String inputMethod = InputMethods.getCurrentInputMethod(mContext);
        if (!isEmpty && sInputMethodBlocklist.contains(inputMethod)) {
            // Don't display the form auto-complete popup after the user starts typing
            // to avoid confusing somes IME. See bug 758820 and bug 632744.
            hide();
            return;
        }

        if (mAutoCompleteList == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            mAutoCompleteList = (ListView) inflater.inflate(R.layout.autocomplete_list, null);

            mAutoCompleteList.setOnItemClickListener(new OnItemClickListener() {
                @Override
                public void onItemClick(AdapterView<?> parentView, View view, int position, long id) {
                    // Use the value stored with the autocomplete view, not the label text,
                    // since they can be different.
                    final TextView textView = (TextView) view;
                    final GeckoBundle message = new GeckoBundle(1);
                    message.putString("value", (String) textView.getTag());
                    GeckoApp.getEventDispatcher().dispatch("FormAssist:AutoComplete", message);
                    hide();
                }
            });

            // Create a ListView-specific touch listener. ListViews are given special treatment because
            // by default they handle touches for their list items... i.e. they're in charge of drawing
            // the pressed state (the list selector), handling list item clicks, etc.
            final SwipeDismissListViewTouchListener touchListener = new SwipeDismissListViewTouchListener(mAutoCompleteList, new OnDismissCallback() {
                @Override
                public void onDismiss(ListView listView, final int position) {
                    // Use the value stored with the autocomplete view, not the label text,
                    // since they can be different.
                    AutoCompleteListAdapter adapter = (AutoCompleteListAdapter) listView.getAdapter();
                    Pair<String, String> item = adapter.getItem(position);

                    // Remove the item from form history.
                    final GeckoBundle message = new GeckoBundle(1);
                    message.putString("value", item.second);
                    GeckoApp.getEventDispatcher().dispatch("FormAssist:Remove", message);

                    // Update the list
                    adapter.remove(item);
                    adapter.notifyDataSetChanged();
                    positionAndShowPopup();
                }
            });
            mAutoCompleteList.setOnTouchListener(touchListener);

            // Setting this scroll listener is required to ensure that during ListView scrolling,
            // we don't look for swipes.
            mAutoCompleteList.setOnScrollListener(touchListener.makeScrollListener());

            // Setting this recycler listener is required to make sure animated views are reset.
            mAutoCompleteList.setRecyclerListener(touchListener.makeRecyclerListener());

            addView(mAutoCompleteList);
        }

        AutoCompleteListAdapter adapter = new AutoCompleteListAdapter(mContext, R.layout.autocomplete_list_item);
        adapter.populateSuggestionsList(suggestions);
        mAutoCompleteList.setAdapter(adapter);

        if (setGeckoPositionData(rect, true)) {
            positionAndShowPopup();
        }
    }

    private void showValidationMessage(final String validationMessage,
                                       final GeckoBundle rect) {
        if (mValidationMessage == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            mValidationMessage = (RelativeLayout) inflater.inflate(R.layout.validation_message, null);

            addView(mValidationMessage);
            mValidationMessageText = (TextView) mValidationMessage.findViewById(R.id.validation_message_text);

            sValidationTextMarginTop = (int) (mContext.getResources().getDimension(R.dimen.validation_message_margin_top));

            sValidationTextLayoutNormal = new LayoutParams(mValidationMessageText.getLayoutParams());
            sValidationTextLayoutNormal.setMargins(0, sValidationTextMarginTop, 0, 0);

            sValidationTextLayoutInverted = new LayoutParams((ViewGroup.MarginLayoutParams) sValidationTextLayoutNormal);
            sValidationTextLayoutInverted.setMargins(0, 0, 0, 0);

            mValidationMessageArrow = (ImageView) mValidationMessage.findViewById(R.id.validation_message_arrow);
            mValidationMessageArrowInverted = (ImageView) mValidationMessage.findViewById(R.id.validation_message_arrow_inverted);
        }

        mValidationMessageText.setText(validationMessage);

        // We need to set the text as selected for the marquee text to work.
        mValidationMessageText.setSelected(true);

        if (setGeckoPositionData(rect, false)) {
            positionAndShowPopup();
        }
    }

    private boolean setGeckoPositionData(final GeckoBundle rect,
                                         final boolean isAutoComplete) {
        mX = rect.getDouble("x");
        mY = rect.getDouble("y");
        mW = rect.getDouble("w");
        mH = rect.getDouble("h");
        mPopupType = (isAutoComplete ?
                      PopupType.AUTOCOMPLETE : PopupType.VALIDATIONMESSAGE);
        return true;
    }

    private void positionAndShowPopup() {
        positionAndShowPopup(GeckoAppShell.getLayerView().getViewportMetrics());
    }

    private void positionAndShowPopup(ImmutableViewportMetrics aMetrics) {
        ThreadUtils.assertOnUiThread();

        // Don't show the form assist popup when using fullscreen VKB
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm.isFullscreenMode()) {
            return;
        }

        // Hide/show the appropriate popup contents
        if (mAutoCompleteList != null) {
            mAutoCompleteList.setVisibility((mPopupType == PopupType.AUTOCOMPLETE) ? VISIBLE : GONE);
        }
        if (mValidationMessage != null) {
            mValidationMessage.setVisibility((mPopupType == PopupType.AUTOCOMPLETE) ? GONE : VISIBLE);
        }

        if (sAutoCompleteMinWidth == 0) {
            Resources res = mContext.getResources();
            sAutoCompleteMinWidth = (int) (res.getDimension(R.dimen.autocomplete_min_width));
            sAutoCompleteRowHeight = (int) (res.getDimension(R.dimen.autocomplete_row_height));
            sValidationMessageHeight = (int) (res.getDimension(R.dimen.validation_message_height));
        }

        float zoom = aMetrics.zoomFactor;

        // These values correspond to the input box for which we want to
        // display the FormAssistPopup.
        int left = (int) (mX * zoom - aMetrics.viewportRectLeft);
        int top = (int) (mY * zoom - aMetrics.viewportRectTop + GeckoAppShell.getLayerView().getCurrentToolbarHeight());
        int width = (int) (mW * zoom);
        int height = (int) (mH * zoom);

        int popupWidth = LayoutParams.MATCH_PARENT;
        int popupLeft = left < 0 ? 0 : left;

        FloatSize viewport = aMetrics.getSize();

        // For autocomplete suggestions, if the input is smaller than the screen-width,
        // shrink the popup's width. Otherwise, keep it as MATCH_PARENT.
        if ((mPopupType == PopupType.AUTOCOMPLETE) && (left + width) < viewport.width) {
            popupWidth = left < 0 ? left + width : width;

            // Ensure the popup has a minimum width.
            if (popupWidth < sAutoCompleteMinWidth) {
                popupWidth = sAutoCompleteMinWidth;

                // Move the popup to the left if there isn't enough room for it.
                if ((popupLeft + popupWidth) > viewport.width) {
                    popupLeft = (int) (viewport.width - popupWidth);
                }
            }
        }

        int popupHeight;
        if (mPopupType == PopupType.AUTOCOMPLETE) {
            // Limit the amount of visible rows.
            int rows = mAutoCompleteList.getAdapter().getCount();
            if (rows > MAX_VISIBLE_ROWS) {
                rows = MAX_VISIBLE_ROWS;
            }

            popupHeight = sAutoCompleteRowHeight * rows;
        } else {
            popupHeight = sValidationMessageHeight;
        }

        int popupTop = top + height;

        if (mPopupType == PopupType.VALIDATIONMESSAGE) {
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

                if (mPopupType == PopupType.VALIDATIONMESSAGE) {
                    mValidationMessageText.setLayoutParams(sValidationTextLayoutInverted);
                    mValidationMessageArrow.setVisibility(GONE);
                    mValidationMessageArrowInverted.setVisibility(VISIBLE);
                }
           }
        }

        LayoutParams layoutParams = new LayoutParams(popupWidth, popupHeight);
        layoutParams.setMargins(popupLeft, popupTop, 0, 0);
        setLayoutParams(layoutParams);
        requestLayout();

        if (!isShown()) {
            setVisibility(VISIBLE);
            startAnimation(mAnimation);
        }
    }

    public void hide() {
        if (isShown()) {
            setVisibility(GONE);
            GeckoApp.getEventDispatcher().dispatch("FormAssist:Hidden", null);
        }
    }

    void onTranslationChanged() {
        ThreadUtils.assertOnUiThread();
        if (!isShown()) {
            return;
        }
        positionAndShowPopup();
    }

    void onMetricsChanged(final ImmutableViewportMetrics aMetrics) {
        if (!isShown()) {
            return;
        }

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                positionAndShowPopup(aMetrics);
            }
        });
    }

    private class AutoCompleteListAdapter extends ArrayAdapter<Pair<String, String>> {
        private final LayoutInflater mInflater;
        private final int mTextViewResourceId;

        public AutoCompleteListAdapter(Context context, int textViewResourceId) {
            super(context, textViewResourceId);

            mInflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            mTextViewResourceId = textViewResourceId;
        }

        // This method takes an array of autocomplete suggestions with label/value properties
        // and adds label/value Pair objects to the array that backs the adapter.
        public void populateSuggestionsList(final GeckoBundle[] suggestions) {
            for (int i = 0; i < suggestions.length; i++) {
                final GeckoBundle suggestion = suggestions[i];
                final String label = suggestion.getString("label");
                final String value = suggestion.getString("value");
                add(new Pair<String, String>(label, value));
            }
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = mInflater.inflate(mTextViewResourceId, null);
            }

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
