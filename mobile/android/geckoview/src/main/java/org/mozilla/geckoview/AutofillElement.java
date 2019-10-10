/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.graphics.Rect;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.v4.util.ArrayMap;
import android.view.View;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.Map;

/**
 * Represents a single autofill element.
 */
public class AutofillElement {

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({HINT_NONE, HINT_EMAIL_ADDRESS, HINT_PASSWORD, HINT_URL, HINT_USERNAME})
    /* package */ @interface AutofillHint {}

    /**
     * Hint indicating that no special handling is required.
     */
    public static final int HINT_NONE = -1;

    /**
     * Hint indicating that an element represents an email address.
     */
    public static final int HINT_EMAIL_ADDRESS = 0;

    /**
     * Hint indicating that an element represents a password.
     */
    public static final int HINT_PASSWORD = 1;

    /**
     * Hint indicating that an element represents an URL.
     */
    public static final int HINT_URL = 2;

    /**
     * Hint indicating that an element represents a username.
     */
    public static final int HINT_USERNAME = 3;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({INPUT_TYPE_NONE, INPUT_TYPE_TEXT, INPUT_TYPE_NUMBER, INPUT_TYPE_PHONE})
    /* package */ @interface AutofillInputType {}

    /**
     * Indicates that an element is not a known input type.
     */
    public static final int INPUT_TYPE_NONE = -1;

    /**
     * Indicates that an element is a text input type (e.g., {@code <input type="text">})
     */
    public static final int INPUT_TYPE_TEXT = 0;

    /**
     * Indicates that an element is a number input type (e.g., {@code <input type="number">})
     */
    public static final int INPUT_TYPE_NUMBER = 1;

    /**
     * Indicates that an element is a phone input type (e.g., {@code <input type="tel">})
     */
    public static final int INPUT_TYPE_PHONE = 2;

    /**
     * A unique (within this page) id for this element.
     */
    public final int id;

    /**
     * The dimensions of this element in CSS coordinates.
     */
    public final @NonNull Rect dimensions;

    /**
     * The collection of child elements for this element.
     */
    public final @NonNull Collection<AutofillElement> children;

    /**
     * The HTML attributes for this element.
     */
    public final @NonNull Map<String, String> attributes;

    /**
     * Whether or not this element is enabled.
     */
    public final boolean enabled;

    /**
     * Whether or not this element is focusable.
     */
    public final boolean focusable;

    /**
     * Whether or not this element is focused.
     */
    public final boolean focused;

    /**
     * A hint for the type of data contained in this element, if any.
     */
    public final @AutofillHint int hint;

    /**
     * The input type of this element, if any.
     */
    public final @AutofillInputType int inputType;

    /**
     * The HTML tag for this element.
     */
    public final @NonNull String tag;

    /**
     * The web domain for this element.
     */
    public final @NonNull String domain;

    private AutofillElement(final Builder builder) {
        id = builder.mId;
        dimensions = builder.mDimensions != null ? builder.mDimensions : new Rect(0, 0, 0, 0);
        attributes = Collections.unmodifiableMap(builder.mAttributes != null ? builder.mAttributes : new ArrayMap<>());
        enabled = builder.mEnabled;
        focusable = builder.mFocusable;
        focused = builder.mFocused;
        hint = builder.mHint;
        inputType = builder.mInputType;
        tag = builder.mTag;
        domain = builder.mDomain;

        if (builder.mChildren != null) {
            LinkedList<AutofillElement> children = new LinkedList<>();
            for (Builder child : builder.mChildren) {
                children.add(child.build());
            }
            this.children = children;
        } else {
            this.children = new LinkedList<>();
        }
    }

    protected AutofillElement() {
        id = View.NO_ID;
        dimensions = new Rect(0, 0, 0, 0);
        attributes = Collections.unmodifiableMap(new ArrayMap<>());
        enabled = false;
        focusable = false;
        focused = false;
        hint = HINT_NONE;
        inputType = INPUT_TYPE_NONE;
        tag = "";
        domain = "";
        children = new LinkedList<>();
    }

    /* package */ static class Builder {
        private int mId = View.NO_ID;
        private Rect mDimensions;
        private LinkedList<Builder> mChildren;
        private ArrayMap<String, String> mAttributes;
        private boolean mEnabled;
        private boolean mFocusable;
        private boolean mFocused;
        private int mHint = HINT_NONE;
        private int mInputType = INPUT_TYPE_NONE;
        private String mTag = "";
        private String mDomain = "";

        public void dimensions(final Rect rect) {
            mDimensions = rect;
        }

        public AutofillElement build() {
            return new AutofillElement(this);
        }

        public void id(final int id) {
            mId = id;
        }

        public Builder child() {
            if (mChildren == null) {
                mChildren = new LinkedList<>();
            }

            final Builder child = new Builder();
            mChildren.add(child);
            return child;
        }

        public void attribute(final String key, final String value) {
            if (mAttributes == null) {
                mAttributes = new ArrayMap<>();
            }

            mAttributes.put(key, value);
        }

        public void enabled(final boolean enabled) {
            mEnabled = enabled;
        }

        public void focusable(final boolean focusable) {
            mFocusable = focusable;
        }

        public void focused(final boolean focused) {
            mFocused = focused;
        }

        public void hint(final int hint) {
            mHint = hint;
        }

        public void inputType(final int inputType) {
            mInputType = inputType;
        }

        public void tag(final String tag) {
            mTag = tag;
        }

        public void domain(final String domain) {
            mDomain = domain;
        }
    }
}
