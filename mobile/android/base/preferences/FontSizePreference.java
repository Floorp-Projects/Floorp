/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.R;

import android.app.AlertDialog;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.util.Log;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.HashMap;

class FontSizePreference extends DialogPreference {
    private static final String LOGTAG = "FontSizePreference";
    private static final int TWIP_TO_PT_RATIO = 20; // 20 twip = 1 point.
    private static final int PREVIEW_FONT_SIZE_UNIT = TypedValue.COMPLEX_UNIT_PT;
    private static final int DEFAULT_FONT_INDEX = 2;

    private final Context mContext;
    /** Container for mPreviewFontView to allow for scrollable padding at the top of the view. */
    private ScrollView mScrollingContainer;
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
    }

    @Override
    protected void onPrepareDialogBuilder(AlertDialog.Builder builder) {
        final LayoutInflater inflater =
            (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View dialogView = inflater.inflate(R.layout.font_size_preference, null);
        initInternalViews(dialogView);
        updatePreviewFontSize(mFontTwipValues[mPreviewFontIndex]);

        builder.setTitle(null);
        builder.setView(dialogView);
    }

    /** Saves relevant views to instance variables and initializes their settings. */
    private void initInternalViews(View dialogView) {
        mScrollingContainer = (ScrollView) dialogView.findViewById(R.id.scrolling_container);
        // Background cannot be set in XML (see bug 783597 - TODO: Change this to XML when bug is fixed).
        mScrollingContainer.setBackgroundColor(Color.WHITE);
        mPreviewFontView = (TextView) dialogView.findViewById(R.id.preview);

        mDecreaseFontButton = (Button) dialogView.findViewById(R.id.decrease_preview_font_button);
        mIncreaseFontButton = (Button) dialogView.findViewById(R.id.increase_preview_font_button);
        setButtonState(mPreviewFontIndex);
        mDecreaseFontButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mPreviewFontIndex = Math.max(mPreviewFontIndex - 1, 0);
                updatePreviewFontSize(mFontTwipValues[mPreviewFontIndex]);
                mIncreaseFontButton.setEnabled(true);
                // If we reached the minimum index, disable the button.
                if (mPreviewFontIndex == 0) {
                    mDecreaseFontButton.setEnabled(false);
                }
            }
        });
        mIncreaseFontButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mPreviewFontIndex = Math.min(mPreviewFontIndex + 1, mFontTwipValues.length - 1);
                updatePreviewFontSize(mFontTwipValues[mPreviewFontIndex]);

                mDecreaseFontButton.setEnabled(true);
                // If we reached the maximum index, disable the button.
                if (mPreviewFontIndex == mFontTwipValues.length - 1) {
                    mIncreaseFontButton.setEnabled(false);
                }
            }
        });
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
     * Updates the mPreviewFontView to the given text size, resets the container's scroll to the top
     * left, and invalidates the view. Does not update the font indices.
     */
    private void updatePreviewFontSize(String twip) {
        float pt = convertTwipStrToPT(twip);
        // Android will not render a font size of 0 pt but for Gecko, 0 twip turns off font
        // inflation. Thus we special case 0 twip to display a renderable font size.
        if (pt == 0) {
            // Android adds an inexplicable extra margin on the smallest font size so to get around
            // this, we reinflate the view.
            ViewGroup parentView = (ViewGroup) mScrollingContainer.getParent();
            parentView.removeAllViews();
            final LayoutInflater inflater =
                (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            View dialogView = inflater.inflate(R.layout.font_size_preference, parentView);
            initInternalViews(dialogView);
            mPreviewFontView.setTextSize(PREVIEW_FONT_SIZE_UNIT, 1);
        } else {
            mPreviewFontView.setTextSize(PREVIEW_FONT_SIZE_UNIT, pt);
        }
        mScrollingContainer.scrollTo(0, 0);
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
