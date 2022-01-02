/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test.util;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import androidx.annotation.NonNull;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicBoolean;
import org.mozilla.geckoview.GeckoResult;

public class UiThreadUtils {
  private static Method sGetNextMessage = null;

  static {
    try {
      sGetNextMessage = MessageQueue.class.getDeclaredMethod("next");
      sGetNextMessage.setAccessible(true);
    } catch (final NoSuchMethodException e) {
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
   * This waits for the given result and returns it's value. If the result failed with an exception,
   * it is rethrown.
   *
   * @param result A {@link GeckoResult} instance.
   * @param <T> The type of the value held by the {@link GeckoResult}
   * @return The value of the completed {@link GeckoResult}.
   */
  public static <T> T waitForResult(@NonNull final GeckoResult<T> result, final long timeout)
      throws Throwable {
    final ResultHolder<T> holder = new ResultHolder<>(result);

    waitForCondition(() -> holder.isComplete, timeout);

    if (holder.error != null) {
      throw holder.error;
    }

    return holder.value;
  }

  private static class ResultHolder<T> {
    public T value;
    public Throwable error;
    public boolean isComplete;

    public ResultHolder(final GeckoResult<T> result) {
      result.accept(
          value -> {
            ResultHolder.this.value = value;
            isComplete = true;
          },
          error -> {
            ResultHolder.this.error = error;
            isComplete = true;
          });
    }
  }

  public interface Condition {
    boolean test();
  }

  public static void loopUntilIdle(final long timeout) {
    final AtomicBoolean idle = new AtomicBoolean(false);

    MessageQueue.IdleHandler handler = null;
    try {
      handler =
          () -> {
            idle.set(true);
            // Remove handler
            return false;
          };

      HANDLER.getLooper().getQueue().addIdleHandler(handler);

      waitForCondition(() -> idle.get(), timeout);
    } finally {
      if (handler != null) {
        HANDLER.getLooper().getQueue().removeIdleHandler(handler);
      }
    }
  }

  public static void waitForCondition(final Condition condition, final long timeout) {
    // Adapted from GeckoThread.pumpMessageLoop.
    final MessageQueue queue = HANDLER.getLooper().getQueue();

    TIMEOUT_RUNNABLE.set(timeout);

    MessageQueue.IdleHandler handler = null;
    try {
      handler =
          () -> {
            HANDLER.postDelayed(() -> {}, 100);
            return true;
          };

      HANDLER.getLooper().getQueue().addIdleHandler(handler);
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
      if (handler != null) {
        HANDLER.getLooper().getQueue().removeIdleHandler(handler);
      }
    }
  }
}
