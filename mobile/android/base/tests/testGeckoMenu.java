package org.mozilla.gecko.tests;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.util.ThreadUtils;

public class testGeckoMenu extends BaseTest {

    private volatile Exception exception;
    private void setException(Exception e) {
        this.exception = e;
    }

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testMenuThreading() throws InterruptedException {
        final GeckoMenu menu = new GeckoMenu(mSolo.getCurrentActivity());
        final Object semaphore = new Object();

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    menu.add("test1");
                } catch (Exception e) {
                    setException(e);
                }

                synchronized (semaphore) {
                    semaphore.notify();
                }
            }
        });
        synchronized (semaphore) {
            semaphore.wait();
        }
        mAsserter.is(exception, null, "No exception thrown if called on UI thread.");

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    menu.add("test2");
                } catch (Exception e) {
                    setException(e);
                }

                synchronized (semaphore) {
                    semaphore.notify();
                }
            }
        }).start();
        synchronized (semaphore) {
            semaphore.wait();
        }

        if (AppConstants.RELEASE_BUILD) {
            mAsserter.is(exception, null, "No exception was thrown: release build.");
            return;
        }

        mAsserter.is(exception != null, true, "An exception was thrown.");
        mAsserter.is(exception.getClass(), IllegalThreadStateException.class, "An IllegalThreadStateException was thrown.");
    }
}
