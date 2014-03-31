/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import org.mozilla.gecko.Assert;

import junit.framework.AssertionFailedError;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Route messages from Javascript's head.js test framework into Java's
 * Mochitest framework.
 */
public final class JavascriptMessageParser {

    /**
    * The Javascript test harness sends test events to Java.
    * Each such test event is wrapped in a Robocop:JS event.
    */
    public static final String EVENT_TYPE = "Robocop:JS";

    // Messages matching this pattern are handled specially.  Messages not
    // matching this pattern are still printed.
    private static final Pattern testMessagePattern =
        Pattern.compile("\n+TEST-(.*) \\| (.*) \\| (.*)\n*");

    private final Assert asserter;
    // Used to help print stack traces neatly.
    private String lastTestName = "";
    // Have we seen a message saying the test is finished?
    private boolean testFinishedMessageSeen = false;

    public JavascriptMessageParser(final Assert asserter) {
        this.asserter = asserter;
    }

    public boolean isTestFinished() {
        return testFinishedMessageSeen;
    }

    public void logMessage(final String str) {
        final Matcher m = testMessagePattern.matcher(str);

        if (m.matches()) {
            final String type = m.group(1);
            final String name = m.group(2);
            final String message = m.group(3);

            if ("INFO".equals(type)) {
                asserter.info(name, message);
                testFinishedMessageSeen = testFinishedMessageSeen ||
                                          "exiting test".equals(message);
            } else if ("PASS".equals(type)) {
                asserter.ok(true, name, message);
            } else if ("UNEXPECTED-FAIL".equals(type)) {
                try {
                    asserter.ok(false, name, message);
                } catch (AssertionFailedError e) {
                    // Swallow this exception.  We want to see all the
                    // Javascript failures, not die on the very first one!
                }
            } else if ("KNOWN-FAIL".equals(type)) {
                asserter.todo(false, name, message);
            } else if ("UNEXPECTED-PASS".equals(type)) {
                asserter.todo(true, name, message);
            }

            lastTestName = name;
        } else {
            // Generally, these extra lines are stack traces from failures,
            // so we print them with the name of the last test seen.
            asserter.info(lastTestName, str.trim());
        }
    }
}
