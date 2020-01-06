package org.mozilla.geckoview.test.rule;

/**
 * Exception thrown when an error occurs in the test harness itself and not in a specific test
 */
public class TestHarnessException extends RuntimeException {
    public TestHarnessException(final Throwable cause) {
        super(cause);
    }
}
