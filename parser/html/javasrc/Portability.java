/*
 * Copyright (c) 2008-2015 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

package nu.validator.htmlparser.impl;

import nu.validator.htmlparser.annotation.Literal;
import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.NoLength;
import nu.validator.htmlparser.common.Interner;

public final class Portability {

    // Allocating methods

    /**
     * Allocates a new local name object. In C++, the refcount must be set up in such a way that
     * calling <code>releaseLocal</code> on the return value balances the refcount set by this method.
     */
    public static @Local String newLocalNameFromBuffer(@NoLength char[] buf, int length, Interner interner) {
        return new String(buf, 0, length).intern();
    }

    public static String newStringFromBuffer(@NoLength char[] buf, int offset, int length
        // CPPONLY: , TreeBuilder treeBuilder, boolean maybeAtomize
    ) {
        return new String(buf, offset, length);
    }

    public static String newEmptyString() {
        return "";
    }

    public static String newStringFromLiteral(@Literal String literal) {
        return literal;
    }

    public static String newStringFromString(String string) {
        return string;
    }

    // XXX get rid of this
    public static char[] newCharArrayFromLocal(@Local String local) {
        return local.toCharArray();
    }

    public static char[] newCharArrayFromString(String string) {
        return string.toCharArray();
    }

    // Deallocation methods

    public static void releaseString(String str) {
        // No-op in Java
    }

    // Comparison methods

    public static boolean localEqualsBuffer(@Local String local, @NoLength char[] buf, int length) {
        if (local.length() != length) {
            return false;
        }
        for (int i = 0; i < length; i++) {
            if (local.charAt(i) != buf[i]) {
                return false;
            }
        }
        return true;
    }

    public static boolean lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(@Literal String lowerCaseLiteral,
            String string) {
        if (string == null) {
            return false;
        }
        if (lowerCaseLiteral.length() > string.length()) {
            return false;
        }
        for (int i = 0; i < lowerCaseLiteral.length(); i++) {
            char c0 = lowerCaseLiteral.charAt(i);
            char c1 = string.charAt(i);
            if (c1 >= 'A' && c1 <= 'Z') {
                c1 += 0x20;
            }
            if (c0 != c1) {
                return false;
            }
        }
        return true;
    }

    public static boolean lowerCaseLiteralEqualsIgnoreAsciiCaseString(@Literal String lowerCaseLiteral,
            String string) {
        if (string == null) {
            return false;
        }
        if (lowerCaseLiteral.length() != string.length()) {
            return false;
        }
        for (int i = 0; i < lowerCaseLiteral.length(); i++) {
            char c0 = lowerCaseLiteral.charAt(i);
            char c1 = string.charAt(i);
            if (c1 >= 'A' && c1 <= 'Z') {
                c1 += 0x20;
            }
            if (c0 != c1) {
                return false;
            }
        }
        return true;
    }

    public static boolean literalEqualsString(@Literal String literal, String string) {
        return literal.equals(string);
    }

    public static boolean stringEqualsString(String one, String other) {
        return one.equals(other);
    }

    public static void delete(Object o) {

    }

    public static void deleteArray(Object o) {

    }
}
