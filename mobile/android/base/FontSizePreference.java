/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.AlertDialog;
import android.content.Context;
import android.content.res.Resources;
import android.preference.DialogPreference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.text.method.ScrollingMovementMethod;
import android.util.AttributeSet;
import android.util.Log;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.HashMap;

class FontSizePreference extends DialogPreference {
    private static final String LOGTAG = "FontSizePreference";
    private static final int TWIP_TO_PT_RATIO = 20; // 20 twip = 1 point.
    private static final int PREVIEW_FONT_SIZE_UNIT = TypedValue.COMPLEX_UNIT_PT;
    private static final int DEFAULT_FONT_INDEX = 2;

    private final Context mContext;
    private TextView mPreviewFontView;
    private Button mIncreaseFontButton;
    private Button mDecreaseFontButton;

    private final String[] mFontTwipValues;
    private final String[] mFontSizeNames; // Ex: "Small".
    /** Index into the above arrays for the saved preference value (from Gecko). */
    private int mSavedFontIndex = DEFAULT_FONT_INDEX;
    /** Index into the above arrays for the currently displayed font size (the preview). */
    private int mPreviewFontIndex = mSavedFontIndex;
    private HashMap<String, Integer> mFontTwipToIndexMap;

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
        mPreviewFontView = (TextView) dialogView.findViewById(R.id.preview);
        mPreviewFontView.setMovementMethod(new ScrollingMovementMethod());

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
     * Updates the mPreviewFontView to the given text size and forces redraw. Does not update the
     * font indicies.
     */
    private void updatePreviewFontSize(String twip) {
        mPreviewFontView.setTextSize(PREVIEW_FONT_SIZE_UNIT, convertTwipStrToPT(twip));
        mPreviewFontView.invalidate();
    }

    /**
     * Resets the font indicies to the default value. Does not update the mPreviewFontView text size.
     */
    private void resetSavedFontSizeToDefault() {
        mSavedFontIndex = DEFAULT_FONT_INDEX;
        mPreviewFontIndex = mSavedFontIndex;
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
