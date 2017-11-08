/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.common;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.background.testhelpers.WaitHelper.InnerError;
import org.mozilla.gecko.background.testhelpers.WaitHelper.TimeoutError;
import org.mozilla.gecko.sync.ThreadPool;

public class TestWaitHelper extends AndroidSyncTestCase {
  private static final String ERROR_UNIQUE_IDENTIFIER = "error unique identifier";

  public static int NO_WAIT    = 1;               // Milliseconds.
  public static int SHORT_WAIT = 100;             // Milliseconds.
  public static int LONG_WAIT  = 3 * SHORT_WAIT;

  private Object notifyMonitor = new Object();
  // Guarded by notifyMonitor.
  private boolean performNotifyCalled      = false;
  private boolean performNotifyErrorCalled = false;
  private void setPerformNotifyCalled() {
    synchronized (notifyMonitor) {
      performNotifyCalled = true;
    }
  }
  private void setPerformNotifyErrorCalled() {
    synchronized (notifyMonitor) {
      performNotifyErrorCalled = true;
    }
  }
  private void resetNotifyCalled() {
    synchronized (notifyMonitor) {
      performNotifyCalled      = false;
      performNotifyErrorCalled = false;
    }
  }
  private void assertBothCalled() {
    synchronized (notifyMonitor) {
      assertTrue(performNotifyCalled);
      assertTrue(performNotifyErrorCalled);
    }
  }
  private void assertErrorCalled() {
    synchronized (notifyMonitor) {
      assertFalse(performNotifyCalled);
      assertTrue(performNotifyErrorCalled);
    }
  }
  private void assertCalled() {
    synchronized (notifyMonitor) {
      assertTrue(performNotifyCalled);
      assertFalse(performNotifyErrorCalled);
    }
  }

  public WaitHelper waitHelper;

  public TestWaitHelper() {
    super();
  }

  public void setUp() {
    WaitHelper.resetTestWaiter();
    waitHelper = WaitHelper.getTestWaiter();
    resetNotifyCalled();
  }

  public void tearDown() {
    assertTrue(waitHelper.isIdle());
  }

  public Runnable performNothingRunnable() {
    return new Runnable() {
      public void run() {
      }
    };
  }

  public Runnable performNotifyRunnable() {
    return new Runnable() {
      public void run() {
        setPerformNotifyCalled();
        waitHelper.performNotify();
      }
    };
  }

  public Runnable performNotifyAfterDelayRunnable(final int delayInMillis) {
    return new Runnable() {
      public void run() {
        try {
          Thread.sleep(delayInMillis);
        } catch (InterruptedException e) {
          fail("Interrupted.");
        }

        setPerformNotifyCalled();
        waitHelper.performNotify();
      }
    };
  }

  public Runnable performNotifyErrorRunnable() {
    return new Runnable() {
      public void run() {
        setPerformNotifyCalled();
        waitHelper.performNotify(new AssertionFailedError(ERROR_UNIQUE_IDENTIFIER));
      }
    };
  }

  public Runnable inThreadPool(final Runnable runnable) {
    return new Runnable() {
      @Override
      public void run() {
        ThreadPool.run(runnable);
      }
    };
  }

  public Runnable inThread(final Runnable runnable) {
    return new Runnable() {
      @Override
      public void run() {
        new Thread(runnable).start();
      }
    };
  }

  protected void expectAssertionFailedError(Runnable runnable) {
    try {
      waitHelper.performWait(runnable);
    } catch (InnerError e) {
      AssertionFailedError inner = (AssertionFailedError)e.innerError;
      setPerformNotifyErrorCalled();
      String message = inner.getMessage();
      assertTrue("Expected '" + message + "' to contain '" + ERROR_UNIQUE_IDENTIFIER + "'",
                 message.contains(ERROR_UNIQUE_IDENTIFIER));
    }
  }

  protected void expectAssertionFailedErrorAfterDelay(int wait, Runnable runnable) {
    try {
      waitHelper.performWait(wait, runnable);
    } catch (InnerError e) {
      AssertionFailedError inner = (AssertionFailedError)e.innerError;
      setPerformNotifyErrorCalled();
      String message = inner.getMessage();
      assertTrue("Expected '" + message + "' to contain '" + ERROR_UNIQUE_IDENTIFIER + "'",
                 message.contains(ERROR_UNIQUE_IDENTIFIER));
    }
  }

  public void testPerformWait() {
    waitHelper.performWait(performNotifyRunnable());
    assertCalled();
  }

  public void testPerformWaitInThread() {
    waitHelper.performWait(inThread(performNotifyRunnable()));
    assertCalled();
  }

  public void testPerformWaitInThreadPool() {
    waitHelper.performWait(inThreadPool(performNotifyRunnable()));
    assertCalled();
  }

  public void testPerformTimeoutWait() {
    waitHelper.performWait(SHORT_WAIT, performNotifyRunnable());
    assertCalled();
  }

  public void testPerformTimeoutWaitInThread() {
    waitHelper.performWait(SHORT_WAIT, inThread(performNotifyRunnable()));
    assertCalled();
  }

  public void testPerformTimeoutWaitInThreadPool() {
    waitHelper.performWait(SHORT_WAIT, inThreadPool(performNotifyRunnable()));
    assertCalled();
  }

  public void testPerformErrorWaitInThread() {
    expectAssertionFailedError(inThread(performNotifyErrorRunnable()));
    assertBothCalled();
  }

  public void testPerformErrorWaitInThreadPool() {
    expectAssertionFailedError(inThreadPool(performNotifyErrorRunnable()));
    assertBothCalled();
  }

  public void testPerformErrorTimeoutWaitInThread() {
    expectAssertionFailedErrorAfterDelay(SHORT_WAIT, inThread(performNotifyErrorRunnable()));
    assertBothCalled();
  }

  public void testPerformErrorTimeoutWaitInThreadPool() {
    expectAssertionFailedErrorAfterDelay(SHORT_WAIT, inThreadPool(performNotifyErrorRunnable()));
    assertBothCalled();
  }

  public void testTimeout() {
    try {
      waitHelper.performWait(SHORT_WAIT, performNothingRunnable());
    } catch (TimeoutError e) {
      setPerformNotifyErrorCalled();
      assertEquals(SHORT_WAIT, e.waitTimeInMillis);
    }
    assertErrorCalled();
  }

  /**
   * This will pass.  The sequence in the main thread is:
   * - A short delay.
   * - performNotify is called.
   * - performWait is called and immediately finds that performNotify was called before.
   */
  public void testDelay() {
    try {
      waitHelper.performWait(1, performNotifyAfterDelayRunnable(SHORT_WAIT));
    } catch (AssertionFailedError e) {
      setPerformNotifyErrorCalled();
      assertTrue(e.getMessage(), e.getMessage().contains("TIMEOUT"));
    }
    assertCalled();
  }

  public Runnable performNotifyMultipleTimesRunnable() {
    return new Runnable() {
      public void run() {
        waitHelper.performNotify();
        setPerformNotifyCalled();
        waitHelper.performNotify();
      }
    };
  }

  public void testPerformNotifyMultipleTimesFails() {
    try {
      waitHelper.performWait(NO_WAIT, performNotifyMultipleTimesRunnable()); // Not run on thread, so runnable executes before performWait looks for notifications.
    } catch (WaitHelper.MultipleNotificationsError e) {
      setPerformNotifyErrorCalled();
    }
    assertBothCalled();
    assertFalse(waitHelper.isIdle()); // First perform notify should be hanging around.
    waitHelper.performWait(NO_WAIT, performNothingRunnable());
  }

  public void testNestedWaitsAndNotifies() {
      waitHelper.performWait(new Runnable() {
        @Override
        public void run() {
          waitHelper.performWait(new Runnable() {
            public void run() {
              setPerformNotifyCalled();
              waitHelper.performNotify();
            }
          });
          setPerformNotifyErrorCalled();
          waitHelper.performNotify();
        }
      });
    assertBothCalled();
  }

  public void testAssertIsReported() {
    try {
      waitHelper.performWait(1, new Runnable() {
        @Override
        public void run() {
          assertTrue("unique identifier", false);
        }
      });
    } catch (AssertionFailedError e) {
      setPerformNotifyErrorCalled();
      assertTrue(e.getMessage(), e.getMessage().contains("unique identifier"));
    }
    assertErrorCalled();
  }

  /**
   * The inner wait will timeout, but the outer wait will succeed.  The sequence in the helper thread is:
   * - A short delay.
   * - performNotify is called.
   *
   * The sequence in the main thread is:
   * - performWait is called and times out because the helper thread does not call
   *   performNotify quickly enough.
   */
  public void testDelayInThread() throws InterruptedException {
    waitHelper.performWait(new Runnable() {
      @Override
      public void run() {
        try {
          waitHelper.performWait(NO_WAIT, inThread(new Runnable() {
            public void run() {
              try {
                Thread.sleep(SHORT_WAIT);
              } catch (InterruptedException e) {
                fail("Interrupted.");
              }

              setPerformNotifyCalled();
              waitHelper.performNotify();
            }
          }));
        } catch (WaitHelper.TimeoutError e) {
          setPerformNotifyErrorCalled();
          assertEquals(NO_WAIT, e.waitTimeInMillis);
        }
      }
    });
    assertBothCalled();
  }

  /**
   * The inner wait will timeout, but the outer wait will succeed.  The sequence in the helper thread is:
   * - A short delay.
   * - performNotify is called.
   *
   * The sequence in the main thread is:
   * - performWait is called and times out because the helper thread does not call
   *   performNotify quickly enough.
   */
  public void testDelayInThreadPool() throws InterruptedException {
    waitHelper.performWait(new Runnable() {
      @Override
      public void run() {
        try {
          waitHelper.performWait(NO_WAIT, inThreadPool(new Runnable() {
            public void run() {
              try {
                Thread.sleep(SHORT_WAIT);
              } catch (InterruptedException e) {
                fail("Interrupted.");
              }

              setPerformNotifyCalled();
              waitHelper.performNotify();
            }
          }));
        } catch (WaitHelper.TimeoutError e) {
          setPerformNotifyErrorCalled();
          assertEquals(NO_WAIT, e.waitTimeInMillis);
        }
      }
    });
    assertBothCalled();
  }
}
