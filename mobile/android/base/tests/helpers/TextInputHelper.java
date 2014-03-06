/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.tests.UITestContext;

import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

/**
 * Provides helper functions for accessing the InputConnection interface
 */
public final class TextInputHelper {

    private TextInputHelper() { /* To disallow instantiation. */ }

    private static ExtractedText getExtractedText(final InputConnection ic) {
        ExtractedTextRequest req = new ExtractedTextRequest();
        ExtractedText extract = ic.getExtractedText(req, 0);
        return extract;
    }

    private static String getText(final InputConnection ic) {
        return getExtractedText(ic).text.toString();
    }

    public static void assertText(final String message,
                                  final InputConnection ic,
                                  final String text) {
        fAssertEquals(message, text, getText(ic));
    }

    public static void assertSelection(final String message,
                                       final InputConnection ic,
                                       final int start,
                                       final int end) {
        ExtractedText extract = getExtractedText(ic);
        fAssertEquals(message, start, extract.selectionStart);
        fAssertEquals(message, end, extract.selectionEnd);
    }

    public static void assertSelectionAt(final String message,
                                         final InputConnection ic,
                                         final int value) {
        assertSelection(message, ic, value, value);
    }

    public static void assertTextAndSelection(final String message,
                                              final InputConnection ic,
                                              final String text,
                                              final int start,
                                              final int end) {
        ExtractedText extract = getExtractedText(ic);
        fAssertEquals(message, text, extract.text);
        fAssertEquals(message, start, extract.selectionStart);
        fAssertEquals(message, end, extract.selectionEnd);
    }

    public static void assertTextAndSelectionAt(final String message,
                                                final InputConnection ic,
                                                final String text,
                                                final int selection) {
        assertTextAndSelection(message, ic, text, selection, selection);
    }
}
