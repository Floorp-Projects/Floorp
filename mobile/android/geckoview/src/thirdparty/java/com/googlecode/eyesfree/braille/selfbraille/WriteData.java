/*
 * Copyright (C) 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package com.googlecode.eyesfree.braille.selfbraille;

import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;

/**
 * Represents what should be shown on the braille display for a
 * part of the accessibility node tree.
 */
public class WriteData implements Parcelable {

    private static final String PROP_SELECTION_START = "selectionStart";
    private static final String PROP_SELECTION_END = "selectionEnd";

    private AccessibilityNodeInfo mAccessibilityNodeInfo;
    private CharSequence mText;
    private Bundle mProperties = Bundle.EMPTY;

    /**
     * Returns a new {@link WriteData} instance for the given {@code view}.
     */
    public static WriteData forView(View view) {
        AccessibilityNodeInfo node = AccessibilityNodeInfo.obtain(view);
        WriteData writeData = new WriteData();
        writeData.mAccessibilityNodeInfo = node;
        return writeData;
    }

    public static WriteData forInfo(AccessibilityNodeInfo info){
        WriteData writeData = new WriteData();
        writeData.mAccessibilityNodeInfo = info;
        return writeData;
    }


    public AccessibilityNodeInfo getAccessibilityNodeInfo() {
        return mAccessibilityNodeInfo;
    }

    /**
     * Sets the text to be displayed when the accessibility node associated
     * with this instance has focus.  If this method is not called (or
     * {@code text} is {@code null}), this client relinquishes control over
     * this node.
     */
    public WriteData setText(CharSequence text) {
        mText = text;
        return this;
    }

    public CharSequence getText() {
        return mText;
    }

    /**
     * Sets the start position in the text of a text selection or cursor that
     * should be marked on the display.  A negative value (the default) means
     * no selection will be added.
     */
    public WriteData setSelectionStart(int v) {
        writableProperties().putInt(PROP_SELECTION_START, v);
        return this;
    }

    /**
     * @see {@link #setSelectionStart}.
     */
    public int getSelectionStart() {
        return mProperties.getInt(PROP_SELECTION_START, -1);
    }

    /**
     * Sets the end of the text selection to be marked on the display.  This
     * value should only be non-negative if the selection start is
     * non-negative.  If this value is <= the selection start, the selection
     * is a cursor.  Otherwise, the selection covers the range from
     * start(inclusive) to end (exclusive).
     *
     * @see {@link android.text.Selection}.
     */
    public WriteData setSelectionEnd(int v) {
        writableProperties().putInt(PROP_SELECTION_END, v);
        return this;
    }

    /**
     * @see {@link #setSelectionEnd}.
     */
    public int getSelectionEnd() {
        return mProperties.getInt(PROP_SELECTION_END, -1);
    }

    private Bundle writableProperties() {
        if (mProperties == Bundle.EMPTY) {
            mProperties = new Bundle();
        }
        return mProperties;
    }

    /**
     * Checks constraints on the fields that must be satisfied before sending
     * this instance to the self braille service.
     * @throws IllegalStateException
     */
    public void validate() throws IllegalStateException {
        if (mAccessibilityNodeInfo == null) {
            throw new IllegalStateException(
                "Accessibility node info can't be null");
        }
        int selectionStart = getSelectionStart();
        int selectionEnd = getSelectionEnd();
        if (mText == null) {
            if (selectionStart > 0 || selectionEnd > 0) {
                throw new IllegalStateException(
                    "Selection can't be set without text");
            }
        } else {
            if (selectionStart < 0 && selectionEnd >= 0) {
                throw new IllegalStateException(
                    "Selection end without start");
            }
            int textLength = mText.length();
            if (selectionStart > textLength || selectionEnd > textLength) {
                throw new IllegalStateException("Selection out of bounds");
            }
        }
    }

    // For Parcelable support.

    public static final Parcelable.Creator<WriteData> CREATOR =
        new Parcelable.Creator<WriteData>() {
            @Override
            public WriteData createFromParcel(Parcel in) {
                return new WriteData(in);
            }

            @Override
            public WriteData[] newArray(int size) {
                return new WriteData[size];
            }
        };

    @Override
    public int describeContents() {
        return 0;
    }

    /**
     * {@inheritDoc}
     * <strong>Note:</strong> The {@link AccessibilityNodeInfo} will be
     * recycled by this method, don't try to use this more than once.
     */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        mAccessibilityNodeInfo.writeToParcel(out, flags);
        // The above call recycles the node, so make sure we don't use it
        // anymore.
        mAccessibilityNodeInfo = null;
        out.writeString(mText.toString());
        out.writeBundle(mProperties);
    }

    private WriteData() {
    }

    private WriteData(Parcel in) {
        mAccessibilityNodeInfo =
                AccessibilityNodeInfo.CREATOR.createFromParcel(in);
        mText = in.readString();
        mProperties = in.readBundle();
    }
}
