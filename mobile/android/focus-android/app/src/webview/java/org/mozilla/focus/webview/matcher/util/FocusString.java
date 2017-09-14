/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webview.matcher.util;

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

    private FocusString(final String string, final int offsetStart, final int offsetEnd) {
        this.string = string;
        this.offsetStart = offsetStart;
        this.offsetEnd = offsetEnd;

        if (offsetStart > offsetEnd || offsetStart < 0 || offsetEnd < 0) {
            throw new StringIndexOutOfBoundsException("Cannot create negative-length String");
        }
    }

    public static FocusString create(final String string) {
        return new ForwardString(string, 0, string.length());
    }

    public int length() {
        return offsetEnd - offsetStart;
    }

    // offset at the start of the  _raw_ input String
    final int offsetStart;
    // offset at the end of the _raw_ input String
    final int offsetEnd;

    @CheckResult public FocusString reverse() {
        if (isReversed()) {
            return new ForwardString(string, offsetStart, offsetEnd);
        } else {
            return new ReverseString(string, offsetStart, offsetEnd);
        }
    }

    public abstract char charAt(final int position);

    public abstract FocusString substring(final int startIndex);

    private static class ForwardString extends FocusString {
        public ForwardString(final String string, final int offsetStart, final int offsetEnd) {
            super(string, offsetStart, offsetEnd);
        }

        @Override
        protected boolean isReversed() {
            return false;
        }

        @Override
        public char charAt(int position) {
            if (position > length()) {
                throw new StringIndexOutOfBoundsException();
            }

            return string.charAt(position + offsetStart);
        }

        @Override
        public FocusString substring(final int startIndex) {
            // Just a normal substring
            return new ForwardString(string, offsetStart + startIndex, offsetEnd);
        }
    }

    private static class ReverseString extends FocusString {
        public ReverseString(final String string, final int offsetStart, final int offsetEnd) {
            super(string, offsetStart, offsetEnd);
        }

        @Override
        protected boolean isReversed() {
            return true;
        }

        @Override
        public char charAt(int position) {
            if (position > length()) {
                throw new StringIndexOutOfBoundsException();
            }

            return string.charAt(length() - 1 - position + offsetStart);
        }

        @Override
        public FocusString substring(int startIndex) {
            return new ReverseString(string, offsetStart, offsetEnd - startIndex);
        }
    }
}
