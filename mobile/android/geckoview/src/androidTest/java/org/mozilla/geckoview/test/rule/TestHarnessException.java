/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test.rule;

/** Exception thrown when an error occurs in the test harness itself and not in a specific test */
public class TestHarnessException extends RuntimeException {
  public TestHarnessException(final Throwable cause) {
    super(cause);
  }
}
