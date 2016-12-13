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
    * Each such test event is wrapped in a Robocop:Java event.
    */
    public static final String EVENT_TYPE = "Robocop:Java";

    // Messages matching this pattern are handled specially.  Messages not
    // matching this pattern are still printed. This pattern should be able
    // to handle having multiple lines in a message.
    private static final Pattern testMessagePattern =
        Pattern.compile("TEST-([A-Z\\-]+) \\| (.*?) \\| (.*)", Pattern.DOTALL);

    private final Assert asserter;
    // Used to help print stack traces neatly.
    private String lastTestName = "";
    // Have we seen a message saying the test is finished?
    private boolean testFinishedMessageSeen = false;
    private final boolean endOnAssertionFailure;

    /**
     * Constructs a message parser for test result messages sent from JavaScript. When seeing an
     * assertion failure, the message parser can use the given {@link org.mozilla.gecko.Assert}
     * instance to immediately end the test (typically if the underlying JS framework is not able
     * to end the test itself) or to swallow the Errors - this functionality is determined by the
     * <code>endOnAssertionFailure</code> parameter.
     *
     * @param asserter The Assert instance to which test results should be passed.
     * @param endOnAssertionFailure
     *        true if the test should end if we see a JS assertion failure, false otherwise.
     */
    public JavascriptMessageParser(final Assert asserter, final boolean endOnAssertionFailure) {
        this.asserter = asserter;
        this.endOnAssertionFailure = endOnAssertionFailure;
    }

    public boolean isTestFinished() {
        return testFinishedMessageSeen;
    }

    public void logMessage(final String str) {
        final Matcher m = testMessagePattern.matcher(str.trim());

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
                    // Above, we call the assert, allowing it to log.
                    // Now we can end the test, if applicable.
                    if (this.endOnAssertionFailure) {
                        throw e;
                    }
                    // Otherwise, swallow the Error. The JS framework we're
                    // logging messages from is likely capable of ending tests
                    // when it needs to, and we want to see all of its failures,
                    // not just the first one!
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
