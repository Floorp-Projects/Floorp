/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.AlertDialog;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.preference.DialogPreference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.text.method.ScrollingMovementMethod;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;

import java.util.HashMap;

class FontSizePreference extends DialogPreference {
    private static final String LOGTAG = "FontSizePreference";
    private static final int TWIP_TO_PT_RATIO = 20; // 20 twip = 1 point.
    private static final int PREVIEW_FONT_SIZE_UNIT = TypedValue.COMPLEX_UNIT_PT;
    private static final int DEFAULT_FONT_INDEX = 2;

    // Final line height = line height * line_mult + line_mult;
    private static final float LINE_SPACING_ADD = 0f; // Units unknown.
    private static final float LINE_SPACING_MULT = 1.0f;

    private static final float MIN_TEXTVIEW_WIDTH_DIP = 360; // Width of the Galaxy Nexus (portrait).
    /** The dialog will encompass the minimum width + (1 / scaleFactor) of the remaining space. */
    private static final float TEXTVIEW_WIDTH_SCALE_FACTOR = 3;

    private final Context mContext;
    private int mCurrentOrientation;
    private TextView mPreviewFontView;
    private Button mIncreaseFontButton;
    private Button mDecreaseFontButton;

    private final String[] mFontTwipValues;
    private final String[] mFontSizeNames; // Ex: "Small".
    /** Index into the above arrays for the saved preference value (from Gecko). */
    private int mSavedFontIndex = DEFAULT_FONT_INDEX;
    /** Index into the above arrays for the currently displayed font size (the preview). */
    private int mPreviewFontIndex = mSavedFontIndex;
    private final HashMap<String, Integer> mFontTwipToIndexMap;

    private boolean mPreviewFontViewHeightSet;

    public FontSizePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        final Resources res = mContext.getResources();
        mFontTwipValues = res.getStringArray(R.array.pref_font_size_values);
        mFontSizeNames = res.getStringArray(R.array.pref_font_size_entries);
        mFontTwipToIndexMap = new HashMap<String, Integer>();
        for (int i = 0; i < mFontTwipValues.length; ++i) {
            mFontTwipToIndexMap.put(mFontTwipValues[i], i);
        }
        mCurrentOrientation = res.getConfiguration().orientation;
    }

    @Override
    protected void onPrepareDialogBuilder(AlertDialog.Builder builder) {
        final LayoutInflater inflater =
            (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View dialogView = inflater.inflate(R.layout.font_size_preference, null);
        mPreviewFontView = (TextView) dialogView.findViewById(R.id.preview);
        mPreviewFontView.setMovementMethod(new ScrollingMovementMethod());
        // There is no way to get the value for this padding so we turn it off.
        mPreviewFontView.setIncludeFontPadding(false);
        // Retrieving line spacing is not available until API 16 so we override the values instead.
        mPreviewFontView.setLineSpacing(LINE_SPACING_ADD, LINE_SPACING_MULT);

        mDecreaseFontButton = (Button) dialogView.findViewById(R.id.decrease_preview_font_button);
        mIncreaseFontButton = (Button) dialogView.findViewById(R.id.increase_preview_font_button);
        mDecreaseFontButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                updatePreviewFontSize(mFontTwipValues[--mPreviewFontIndex]);
                mIncreaseFontButton.setEnabled(true);
                // If we reached the minimum index, disable the button.
                if (mPreviewFontIndex == 0) {
                    mDecreaseFontButton.setEnabled(false);
                }
            }
        });
        mIncreaseFontButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                updatePreviewFontSize(mFontTwipValues[++mPreviewFontIndex]);
        
                mDecreaseFontButton.setEnabled(true);
                // If we reached the maximum index, disable the button.
                if (mPreviewFontIndex == mFontTwipValues.length - 1) {
                    mIncreaseFontButton.setEnabled(false);
                }
            }
        });

        // Set mPreviewFontView dimensions.
        setPreviewFontViewWidth();
        setPreviewFontViewHeightListener();
        setFontSizeToMaximum(); // Expects onGlobalLayout() to be called.

        builder.setView(dialogView);
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);
        if (!positiveResult) {
            mPreviewFontIndex = mSavedFontIndex;
            return;
        }
        mSavedFontIndex = mPreviewFontIndex;
        final String twipVal = mFontTwipValues[mSavedFontIndex];
        final OnPreferenceChangeListener prefChangeListener = getOnPreferenceChangeListener();
        if (prefChangeListener == null) {
            Log.e(LOGTAG, "PreferenceChangeListener is null. FontSizePreference will not be saved to Gecko.");
            return;
        }
        prefChangeListener.onPreferenceChange(this, twipVal);
    }

    protected void onConfigurationChanged(Configuration newConfig) {
        if (mCurrentOrientation != newConfig.orientation) {
            mCurrentOrientation = newConfig.orientation;

            // mPreviewFontView will be null if the dialog has not yet been shown.
            if (mPreviewFontView != null) {
                // Recalculate the mPreviewFontView dimensions since we have new screen dimensions.
                setPreviewFontViewWidth();
                mPreviewFontViewHeightSet = false;
                setFontSizeToMaximum(); // Expects onGlobalLayout() to be called.
            }
        }
    }

    /**
     * Sets the preview font size to the maximum given size.
     */
    private void setFontSizeToMaximum() {
        updatePreviewFontSize(mFontTwipValues[mFontTwipValues.length - 1]);
        // NOTE: If this method is being used with onGlobalLayout() to set mFontPreviewView height,
        // the font size cannot be changed past this point or the dialog height will not calculate
        // correctly. We must wait for the global layout state to change, calculate the height in
        // onGlobalLayout(), and only then can changes be made.
    }

    /**
     * Sets a global layout listener that will calculate the maximum required height of
     * mPreviewFontView based upon the current font values and then reset the TextView to the saved
     * preference values. This listener will only run once. mPreviewFontViewHeightSet must be set to
     * false before being used again.
     */
    private void setPreviewFontViewHeightListener() {
        mPreviewFontViewHeightSet = false;
        final ViewTreeObserver vto = mPreviewFontView.getViewTreeObserver(); 
        vto.addOnGlobalLayoutListener(new OnGlobalLayoutListener() { 
            @Override 
            public void onGlobalLayout() { 
                if (!mPreviewFontViewHeightSet) {
                    mPreviewFontViewHeightSet = true;
                    final int desiredHeight = (int) (mPreviewFontView.getLineCount() *
                            mPreviewFontView.getLineHeight() * LINE_SPACING_MULT + LINE_SPACING_ADD +
                            mPreviewFontView.getPaddingTop() + mPreviewFontView.getPaddingBottom());
                    mPreviewFontView.setHeight(desiredHeight);

                    // Set the dialog state to the saved preference values.
                    setButtonState(mPreviewFontIndex);
                    updatePreviewFontSize(mFontTwipValues[mPreviewFontIndex]);
                }
            } 
        }); 
    }

    /**
     * Sets the width of the mPreviewFontView TextView. The width is calculated by adding a constant
     * minimum width to a fraction of the remaining space on screen (if any), which is determined by
     * the given scale factor. Note that in ICS, it appears that the dialog will not wrap content
     * and will set the view size itself. In Gingerbread, this code executes and sets the dialog
     * width dynamically.
     */
    private void setPreviewFontViewWidth() {
        final WindowManager wm = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        final DisplayMetrics metrics = new DisplayMetrics();
        wm.getDefaultDisplay().getMetrics(metrics);
        final float density = metrics.density;

        final float actualWidthDip = metrics.widthPixels / density;
        float scaleExtraDip = (actualWidthDip - MIN_TEXTVIEW_WIDTH_DIP) / TEXTVIEW_WIDTH_SCALE_FACTOR;
        scaleExtraDip = scaleExtraDip >= 0 ? scaleExtraDip : 0;
        final float desiredWidthDip = MIN_TEXTVIEW_WIDTH_DIP + scaleExtraDip;
        final int desiredWidthPx = Math.round(desiredWidthDip * density);
        mPreviewFontView.setWidth(desiredWidthPx);
    }

    /**
     * Finds the index of the given twip value and sets it as the saved preference value. Also the
     * current preview text size to the given value. Does not update the mPreviewFontView text size.
     */
    protected void setSavedFontSize(String twip) {
        final Integer index = mFontTwipToIndexMap.get(twip);
        if (index != null) {
            mSavedFontIndex = index;
            mPreviewFontIndex = mSavedFontIndex;
            return;
        }
        resetSavedFontSizeToDefault();
        Log.e(LOGTAG, "setSavedFontSize: Given font size does not exist in twip values map. Reverted to default font size.");
    }

    /**
     * Updates the mPreviewFontView to the given text size, resets the scroll to the top left, and
     * invalidates the view. Does not update the font indices.
     */
    private void updatePreviewFontSize(String twip) {
        float pt = convertTwipStrToPT(twip);
        // Android will not render a font size of 0 pt but for Gecko, 0 twip turns off font
        // inflation. Thus we special case 0 twip to display a renderable font size.
        if (pt == 0) {
            mPreviewFontView.setTextSize(PREVIEW_FONT_SIZE_UNIT, 1);
        } else {
            mPreviewFontView.setTextSize(PREVIEW_FONT_SIZE_UNIT, pt);
        }
        mPreviewFontView.scrollTo(0, 0);
    }

    /**
     * Resets the font indices to the default value. Does not update the mPreviewFontView text size.
     */
    private void resetSavedFontSizeToDefault() {
        mSavedFontIndex = DEFAULT_FONT_INDEX;
        mPreviewFontIndex = mSavedFontIndex;
    }

    private void setButtonState(int index) {
        if (index == 0) {
            mDecreaseFontButton.setEnabled(false);
        } else if (index == mFontTwipValues.length - 1) {
            mIncreaseFontButton.setEnabled(false);
        }
    }

    /**
     * Returns the name of the font size (ex: "Small") at the currently saved preference value.
     */
    protected String getSavedFontSizeName() {
        return mFontSizeNames[mSavedFontIndex];
    }

    private float convertTwipStrToPT(String twip) {
        return Float.parseFloat(twip) / TWIP_TO_PT_RATIO; 
    }
}
