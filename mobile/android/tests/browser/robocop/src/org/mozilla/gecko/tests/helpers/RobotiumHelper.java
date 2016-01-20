/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import com.jayway.android.robotium.solo.Solo;

import org.mozilla.gecko.tests.UITestContext;

import java.util.regex.Pattern;

/**
 * Provides helper functions for using Robotium.
 */
public final class RobotiumHelper {
    private static Solo sSolo;

    private RobotiumHelper() { /* To disallow instantiation. */ }

    protected static void init(final UITestContext context) {
        sSolo = context.getSolo();
    }

    /**
     * Same as Solo.waitForText(), but matching against full text, without regular expressions.
     */
    public static boolean waitForExactText(final String text,
                                           final int minimumNumberOfMatches,
                                           final long timeout) {
        String matchText = "^" + Pattern.quote(text) + "$";
        return sSolo.waitForText(matchText, minimumNumberOfMatches, timeout);
    }

    /**
     * Same as Solo.searchText(), but matching against full text, without regular expressions.
     */
    public static boolean searchExactText(final String text,
                                          final boolean onlyVisible) {
        String matchText = "^" + Pattern.quote(text) + "$";
        return sSolo.searchText(matchText, onlyVisible);
    }
}
