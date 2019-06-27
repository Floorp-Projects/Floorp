/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.util;

import org.mozilla.geckoview.GeckoResult;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.util.Log;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class UiThreadUtils {
    private static final String LOGTAG = "UiThreadUtils";
    private static long sLongestWait;

    private static Method sGetNextMessage = null;
    static {
        try {
            sGetNextMessage = MessageQueue.class.getDeclaredMethod("next");
            sGetNextMessage.setAccessible(true);
        } catch (NoSuchMethodException e) {
            throw new IllegalStateException(e);
        }
    }

    public static class TimeoutException extends RuntimeException {
        public TimeoutException(final String detailMessage) {
            super(detailMessage);
        }
    }

    private static final class TimeoutRunnable implements Runnable {
        private long timeout;

        public void set(final long timeout) {
            this.timeout = timeout;
            cancel();
            HANDLER.postDelayed(this, timeout);
        }

        public void cancel() {
            HANDLER.removeCallbacks(this);
        }

        @Override
        public void run() {
            throw new TimeoutException("Timed out after " + timeout + "ms");
        }
    }

    public static final Handler HANDLER = new Handler(Looper.getMainLooper());
    private static final TimeoutRunnable TIMEOUT_RUNNABLE = new TimeoutRunnable();
    private static final MessageQueue.IdleHandler IDLE_HANDLER = new MessageQueue.IdleHandler() {
        @Override
        public boolean queueIdle() {
            final Message msg = Message.obtain(HANDLER);
            msg.obj = HANDLER;
            HANDLER.sendMessageAtFrontOfQueue(msg);
            return false; // Remove this idle handler.
        }
    };

    private static RuntimeException unwrapRuntimeException(final Throwable e) {
        final Throwable cause = e.getCause();
        if (cause != null && cause instanceof RuntimeException) {
            return (RuntimeException) cause;
        } else if (e instanceof RuntimeException) {
            return (RuntimeException) e;
        }

        return new RuntimeException(cause != null ? cause : e);
    }

    /**
     * This waits for the given result and returns it's value. If
     * the result failed with an exception, it is rethrown.
     *
     * @param result A {@link GeckoResult} instance.
     * @param <T> The type of the value held by the {@link GeckoResult}
     * @return The value of the completed {@link GeckoResult}.
     */
    public static <T> T waitForResult(@NonNull GeckoResult<T> result, long timeout) throws Throwable {
        final ResultHolder<T> holder = new ResultHolder<>(result);

        while (!holder.isComplete) {
            loopUntilIdle(timeout);
        }

        if (holder.error != null) {
            throw holder.error;
        }

        return holder.value;
    }

    private static class ResultHolder<T> {
        public T value;
        public Throwable error;
        public boolean isComplete;

        public ResultHolder(GeckoResult<T> result) {
            result.then(new GeckoResult.OnValueListener<T, Void>() {
                @Override
                public GeckoResult<Void> onValue(T value) {
                    ResultHolder.this.value = value;
                    isComplete = true;
                    return null;
                }
            }, new GeckoResult.OnExceptionListener<Void>() {
                @Override
                public GeckoResult<Void> onException(Throwable error) {
                    ResultHolder.this.error = error;
                    isComplete = true;
                    return null;
                }
            });
        }
    }

    public interface Condition {
        boolean test();
    }

    public static void waitForCondition(Condition condition, final long timeout) {
         // Adapted from GeckoThread.pumpMessageLoop.
        final MessageQueue queue = HANDLER.getLooper().getQueue();

        TIMEOUT_RUNNABLE.set(timeout);

        try {
            while (!condition.test()) {
                final Message msg;
                try {
                    msg = (Message) sGetNextMessage.invoke(queue);
                } catch (final IllegalAccessException | InvocationTargetException e) {
                    throw unwrapRuntimeException(e);
                }
                if (msg.getTarget() == null) {
                    HANDLER.getLooper().quit();
                    return;
                }
                msg.getTarget().dispatchMessage(msg);
            }
        } finally {
            TIMEOUT_RUNNABLE.cancel();
        }
    }

    public static void loopUntilIdle(final long timeout) {
        // Adapted from GeckoThread.pumpMessageLoop.
        final MessageQueue queue = HANDLER.getLooper().getQueue();
        if (timeout > 0) {
            TIMEOUT_RUNNABLE.set(timeout);
        } else {
            queue.addIdleHandler(IDLE_HANDLER);
        }

        final long startTime = SystemClock.uptimeMillis();
        try {
            while (true) {
                final Message msg;
                try {
                    msg = (Message) sGetNextMessage.invoke(queue);
                } catch (final IllegalAccessException | InvocationTargetException e) {
                    throw unwrapRuntimeException(e);
                }
                if (msg.getTarget() == HANDLER && msg.obj == HANDLER) {
                    // Our idle signal.
                    break;
                } else if (msg.getTarget() == null) {
                    HANDLER.getLooper().quit();
                    return;
                }
                msg.getTarget().dispatchMessage(msg);

                if (timeout > 0) {
                    TIMEOUT_RUNNABLE.cancel();
                    queue.addIdleHandler(IDLE_HANDLER);
                }
            }

            final long waitDuration = SystemClock.uptimeMillis() - startTime;
            if (waitDuration > sLongestWait) {
                sLongestWait = waitDuration;
                Log.i(LOGTAG, "New longest wait: " + waitDuration + "ms");
            }
        } finally {
            if (timeout > 0) {
                TIMEOUT_RUNNABLE.cancel();
            }
        }
    }
}
