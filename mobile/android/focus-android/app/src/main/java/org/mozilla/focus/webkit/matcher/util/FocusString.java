/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit.matcher.util;

import android.support.annotation.CheckResult;

/**
 * A String wrapper utility that allows for efficient string reversal.
 *
 * We regularly need to reverse strings. The standard way of doing this in Java would be to copy
 * the string to reverse (e.g. using StringBuffer.reverse()). This seems wasteful when we only
 * read our Strings character by character, in which case can just transpose positions as needed.
 */
public abstract class FocusString {
    protected final String string;

    protected abstract boolean isReversed();

    private FocusString(final String string) {
        this.string = string;
    }

    public static FocusString create(final String string) {
        return new ForwardString(string);
    }

    public int length() {
        return string.length();
    }

    @CheckResult public FocusString reverse() {
        if (isReversed()) {
            return new ForwardString(string);
        } else {
            return new ReverseString(string);
        }
    }

    public abstract char charAt(final int position);

    public abstract FocusString substring(final int startIndex);

    private static class ForwardString extends FocusString {
        public ForwardString(String string) {
            super(string);
        }

        @Override
        protected boolean isReversed() {
            return false;
        }

        @Override
        public char charAt(int position) {
            return string.charAt(position);
        }

        @Override
        public FocusString substring(final int startIndex) {
            return new ForwardString(string.substring(startIndex));
        }
    }

    private static class ReverseString extends FocusString {
        public ReverseString(String string) {
            super(string);
        }

        @Override
        protected boolean isReversed() {
            return true;
        }

        @Override
        public char charAt(int position) {
            return string.charAt(length() - 1 - position);
        }

        @Override
        public FocusString substring(int startIndex) {
            return new ReverseString(string.substring(0, length() - startIndex));
        }
    }
}
