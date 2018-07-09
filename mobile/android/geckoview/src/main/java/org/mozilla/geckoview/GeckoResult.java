package org.mozilla.geckoview;

import org.mozilla.gecko.util.ThreadUtils;

import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.util.ArrayList;

/**
 * GeckoResult is a class that represents an asynchronous result. The result is initially pending,
 * and at a later time, the result may be completed with {@link #complete a value} or {@link
 * #completeExceptionally an exception} depending on the outcome of the asynchronous operation. For
 * example,<pre>
 * public GeckoResult&lt;Integer&gt; divide(final int dividend, final int divisor) {
 *     final GeckoResult&lt;Integer&gt; result = new GeckoResult&lt;&gt;();
 *     (new Thread(() -&gt; {
 *         if (divisor != 0) {
 *             result.complete(dividend / divisor);
 *         } else {
 *             result.completeExceptionally(new ArithmeticException("Dividing by zero"));
 *         }
 *     })).start();
 *     return result;
 * }</pre>
 * <p>
 * To retrieve the completed value or exception, use one of the {@link #then} methods to register
 * listeners on the result. All listeners are run on the application main thread. For example, to
 * retrieve a completed value,<pre>
 * divide(42, 2).then(new GeckoResult.OnValueListener&lt;Integer, Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onValue(final Integer value) {
 *         // value == 21
 *     }
 * }, new GeckoResult.OnExceptionListener&lt;Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onException(final Throwable exception) {
 *         // Not called
 *     }
 * });</pre>
 * <p>
 * And to retrieve a completed exception,<pre>
 * divide(42, 0).then(new GeckoResult.OnValueListener&lt;Integer, Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onValue(final Integer value) {
 *         // Not called
 *     }
 * }, new GeckoResult.OnExceptionListener&lt;Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onException(final Throwable exception) {
 *         // exception instanceof ArithmeticException
 *     }
 * });</pre>
 * <p>
 * {@link #then} calls may be chained to complete multiple asynchonous operations in sequence.
 * This example takes an integer, converts it to a String, and appends it to another String,<pre>
 * divide(42, 2).then(new GeckoResult.OnValueListener&lt;Integer, String&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;String&gt; onValue(final Integer value) {
 *         return GeckoResult.fromValue(value.toString());
 *     }
 * }).then(new GeckoResult.OnValueListener&lt;String, String&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;String&gt; onValue(final String value) {
 *         return GeckoResult.fromValue("42 / 2 = " + value);
 *     }
 * }).then(new GeckoResult.OnValueListener&lt;String, Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onValue(final String value) {
 *         // value == "42 / 2 = 21"
 *         return null;
 *     }
 * });</pre>
 * <p>
 * Chaining works with exception listeners as well. For example,<pre>
 * divide(42, 0).then(new GeckoResult.OnExceptionListener&lt;String&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onException(final Throwable exception) {
 *         return "foo";
 *     }
 * }).then(new GeckoResult.OnValueListener&lt;String, Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onValue(final String value) {
 *         // value == "foo"
 *     }
 * });</pre>
 * <p>
 * A completed value/exception will propagate down the chain even if an intermediate step does not
 * have a value/exception listener. For example,<pre>
 * divide(42, 0).then(new GeckoResult.OnValueListener&lt;Integer, String&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;String&gt; onValue(final Integer value) {
 *         // Not called
 *     }
 * }).then(new GeckoResult.OnExceptionListener&lt;Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onException(final Throwable exception) {
 *         // exception instanceof ArithmeticException
 *     }
 * });</pre>
 * <p>
 * However, any propagated value will be coerced to null. For example,<pre>
 * divide(42, 2).then(new GeckoResult.OnExceptionListener&lt;String&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;String&gt; onException(final Throwable exception) {
 *         // Not called
 *     }
 * }).then(new GeckoResult.OnValueListener&lt;String, Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onValue(final String value) {
 *         // value == null
 *     }
 * });</pre>
 * <p>
 * Any exception thrown by a listener are automatically used to complete the result. At the end of
 * every chain, there is an implicit exception listener that rethrows any uncaught and unhandled
 * exception as {@link UncaughtException}. The following example will cause {@link
 * UncaughtException} to be thrown because {@code BazException} is uncaught and unhandled at the
 * end of the chain,<pre>
 * GeckoResult.fromValue(42).then(new GeckoResult.OnValueListener&lt;Integer, Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onValue(final Integer value) throws FooException {
 *         throw new FooException();
 *     }
 * }).then(new GeckoResult.OnExceptionListener&lt;Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onException(final Throwable exception) throws Exception {
 *         // exception instanceof FooException
 *         throw new BarException();
 *     }
 * }).then(new GeckoResult.OnExceptionListener&lt;Void&gt;() {
 *     &#64;Override
 *     public GeckoResult&lt;Void&gt; onException(final Throwable exception) throws Throwable {
 *         // exception instanceof BarException
 *         return new BazException();
 *     }
 * });</pre>
 *
 * @param <T> The type of the value delivered via the GeckoResult.
 */
public class GeckoResult<T> {
    private static final String LOGTAG = "GeckoResult";

    public static final class UncaughtException extends RuntimeException {
        public UncaughtException(final Throwable cause) {
            super(cause);
        }
    }

    private boolean mComplete;
    private T mValue;
    private Throwable mError;
    private boolean mIsUncaughtError;
    private ArrayList<Runnable> mListeners;

    /**
     * Construct an incomplete GeckoResult. Call {@link #complete(Object)} or
     * {@link #completeExceptionally(Throwable)} in order to fulfill the result.
     */
    public GeckoResult() {
    }

    /**
     * This constructs a result that is chained to the specified result.
     *
     * @param from The {@link GeckoResult} to copy.
     */
    public GeckoResult(GeckoResult<T> from) {
        this();
        completeFrom(from);
    }

    /**
     * Construct a result that is completed with the specified value.
     *
     * @param value The value used to complete the newly created result.
     * @param <U> Type for the result.
     * @return The completed {@link GeckoResult}
     */
    public static @NonNull <U> GeckoResult<U> fromValue(@Nullable final U value) {
        final GeckoResult<U> result = new GeckoResult<>();
        result.complete(value);
        return result;
    }

    /**
     * Construct a result that is completed with the specified {@link Throwable}.
     * May not be null.
     *
     * @param error The exception used to complete the newly created result.
     * @param <T> Type for the result if the result had been completed without exception.
     * @return The completed {@link GeckoResult}
     */
    public static @NonNull <T> GeckoResult<T> fromException(@NonNull final Throwable error) {
        final GeckoResult<T> result = new GeckoResult<>();
        result.completeExceptionally(error);
        return result;
    }

    @Override
    public synchronized int hashCode() {
        int result = 17;
        result = 31 * result + (mComplete ? 1 : 0);
        result = 31 * result + (mValue != null ? mValue.hashCode() : 0);
        result = 31 * result + (mError != null ? mError.hashCode() : 0);
        return result;
    }

    // This can go away once we can rely on java.util.Objects.equals() (API 19)
    private static boolean objectEquals(final Object a, final Object b) {
        return a == b || (a != null && a.equals(b));
    }

    @Override
    public synchronized boolean equals(final Object other) {
        if (other instanceof GeckoResult<?>) {
            final GeckoResult<?> result = (GeckoResult<?>)other;
            return result.mComplete == mComplete &&
                    objectEquals(result.mError, mError) &&
                    objectEquals(result.mValue, mValue);
        }

        return false;
    }

    /**
     * Convenience method for {@link #then(OnValueListener, OnExceptionListener)}.
     *
     * @param valueListener An instance of {@link OnValueListener}, called when the
     *                      {@link GeckoResult} is completed with a value.
     * @param <U> Type of the new result that is returned by the listener.
     * @return A new {@link GeckoResult} that the listener will complete.
     */
    public @NonNull <U> GeckoResult<U> then(@NonNull final OnValueListener<T, U> valueListener) {
        return then(valueListener, null);
    }

    /**
     * Convenience method for {@link #then(OnValueListener, OnExceptionListener)}.
     *
     * @param exceptionListener An instance of {@link OnExceptionListener}, called when the
     *                          {@link GeckoResult} is completed with an {@link Exception}.
     * @param <U> Type of the new result that is returned by the listener.
     * @return A new {@link GeckoResult} that the listener will complete.
     */
    public @NonNull <U> GeckoResult<U> then(@NonNull final OnExceptionListener<U> exceptionListener) {
        return then(null, exceptionListener);
    }

    /**
     * Adds listeners to be called when the {@link GeckoResult} is completed either with
     * a value or {@link Throwable}. Listeners will be invoked on the main thread. If the
     * result is already complete when this method is called, listeners will be invoked in
     * a future {@link Looper} iteration.
     *
     * @param valueListener An instance of {@link OnValueListener}, called when the
     *                      {@link GeckoResult} is completed with a value.
     * @param exceptionListener An instance of {@link OnExceptionListener}, called when the
     *                          {@link GeckoResult} is completed with an {@link Throwable}.
     * @param <U> Type of the new result that is returned by the listeners.
     * @return A new {@link GeckoResult} that the listeners will complete.
     */
    public @NonNull <U> GeckoResult<U> then(@Nullable final OnValueListener<T, U> valueListener,
                                            @Nullable final OnExceptionListener<U> exceptionListener) {
        if (valueListener == null && exceptionListener == null) {
            throw new IllegalArgumentException("At least one listener should be non-null");
        }

        final GeckoResult<U> result = new GeckoResult<U>();
        then(new Runnable() {
            @Override
            public void run() {
                try {
                    if (haveValue()) {
                        result.completeFrom(valueListener != null ? valueListener.onValue(mValue)
                                                                  : null);
                    } else if (!haveError()) {
                        // Listener called without completion?
                        throw new AssertionError();
                    } else if (exceptionListener != null) {
                        result.completeFrom(exceptionListener.onException(mError));
                    } else {
                        result.mIsUncaughtError = mIsUncaughtError;
                        result.completeExceptionally(mError);
                    }
                } catch (Throwable e) {
                    if (!result.mComplete) {
                        result.mIsUncaughtError = true;
                        result.completeExceptionally(e);
                    }
                }
            }
        });
        return result;
    }

    private synchronized void then(@NonNull final Runnable listener) {
        if (mComplete) {
            dispatchLocked(listener);
        } else {
            if (mListeners == null) {
                mListeners = new ArrayList<>(1);
            }
            mListeners.add(listener);
        }
    }

    private void dispatchLocked() {
        if (!mComplete) {
            throw new IllegalStateException("Cannot dispatch unless result is complete");
        }

        if (mListeners == null && !mIsUncaughtError) {
            return;
        }

        ThreadUtils.getUiHandler().post(new Runnable() {
            @Override
            public void run() {
                if (mListeners != null) {
                    for (final Runnable listener : mListeners) {
                        listener.run();
                    }
                } else if (mIsUncaughtError) {
                    // We have no listeners to forward the uncaught exception to;
                    // rethrow the exception to make it visible.
                    throw new UncaughtException(mError);
                }
            }
        });
    }

    private void dispatchLocked(final Runnable runnable) {
        if (!mComplete) {
            throw new IllegalStateException("Cannot dispatch unless result is complete");
        }

        ThreadUtils.getUiHandler().post(new Runnable() {
            @Override
            public void run() {
                runnable.run();
            }
        });
    }

    /**
     * Completes this result based on another result.
     *
     * @param other The result that this result should mirror
     */
    private void completeFrom(final GeckoResult<T> other) {
        if (other == null) {
            complete(null);
            return;
        }

        other.then(new Runnable() {
            @Override
            public void run() {
                if (other.haveValue()) {
                    complete(other.mValue);
                } else {
                    mIsUncaughtError = other.mIsUncaughtError;
                    completeExceptionally(other.mError);
                }
            }
        });
    }

    /**
     * Complete the result with the specified value. IllegalStateException is thrown
     * if the result is already complete.
     *
     * @param value The value used to complete the result.
     * @throws IllegalStateException If the result is already completed.
     */
    public synchronized void complete(final T value) {
        if (mComplete) {
            throw new IllegalStateException("result is already complete");
        }

        mValue = value;
        mComplete = true;

        dispatchLocked();
    }

    /**
     * Complete the result with the specified {@link Throwable}. IllegalStateException is thrown
     * if the result is already complete.
     *
     * @param exception The {@link Throwable} used to complete the result.
     * @throws IllegalStateException If the result is already completed.
     */
    public synchronized void completeExceptionally(@NonNull final Throwable exception) {
        if (mComplete) {
            throw new IllegalStateException("result is already complete");
        }

        if (exception == null) {
            throw new IllegalArgumentException("Throwable must not be null");
        }

        mError = exception;
        mComplete = true;

        dispatchLocked();
    }

    /**
     * An interface used to deliver values to listeners of a {@link GeckoResult}
     * @param <T> Type of the value delivered via {@link #onValue(Object)}
     * @param <U> Type of the value for the result returned from {@link #onValue(Object)}
     */
    public interface OnValueListener<T, U> {
        /**
         * Called when a {@link GeckoResult} is completed with a value. Will be
         * called on the main thread.
         *
         * @param value The value of the {@link GeckoResult}
         * @return Result used to complete the next result in the chain. May be null.
         * @throws Throwable Exception used to complete next result in the chain.
         */
        @Nullable GeckoResult<U> onValue(@Nullable T value) throws Throwable;
    }

    /**
     * An interface used to deliver exceptions to listeners of a {@link GeckoResult}
     *
     * @param <V> Type of the vale for the result returned from {@link #onException(Throwable)}
     */
    public interface OnExceptionListener<V> {
        /**
         * Called when a {@link GeckoResult} is completed with an exception. Will be
         * called on the main thread.
         *
         * @param exception Exception that completed the result.
         * @return Result used to complete the next result in the chain. May be null.
         * @throws Throwable Exception used to complete next result in the chain.
         */
        @Nullable GeckoResult<V> onException(@NonNull Throwable exception) throws Throwable;
    }

    private boolean haveValue() {
        return mComplete && mError == null;
    }

    private boolean haveError() {
        return mComplete && mError != null;
    }
}
