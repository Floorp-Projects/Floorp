package org.mozilla.geckoview;

import org.mozilla.gecko.util.ThreadUtils;

import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.util.ArrayList;

/**
 * GeckoResult is a class that represents an asynchronous result.
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
     * This constructs an incomplete GeckoResult. Call {@link #complete(Object)} or
     * {@link #completeExceptionally(Throwable)} in order to fulfill the result.
     */
    public GeckoResult() {
    }

    /**
     * This constructs a result from another result. Listeners are not copied.
     *
      * @param from The {@link GeckoResult} to copy.
     */
    public GeckoResult(GeckoResult<T> from) {
        this();
        mComplete = from.mComplete;
        mValue = from.mValue;
        mError = from.mError;
    }

    /**
     * This constructs a result that is completed with the specified value.
     *
     * @param value The value used to complete the newly created result.
     * @return The completed {@link GeckoResult}
     */
    public static @NonNull <U> GeckoResult<U> fromValue(@Nullable final U value) {
        final GeckoResult<U> result = new GeckoResult<>();
        result.complete(value);
        return result;
    }

    /**
     * This constructs a result that is completed with the specified {@link Throwable}.
     * May not be null.
     *
     * @param error The exception used to complete the newly created result.
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
     * @param <U>
     * @return
     */
    public @NonNull <U> GeckoResult<U> then(@NonNull final OnValueListener<T, U> valueListener) {
        return then(valueListener, null);
    }

    /**
     * Convenience method for {@link #then(OnValueListener, OnExceptionListener)}.
     *
     * @param exceptionListener An instance of {@link OnExceptionListener}, called when the
     *                          {@link GeckoResult} is completed with an {@link Exception}.
     * @param <U> The type contained in the returned {@link GeckoResult}
     * @return
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
     * @param <U> The type contained in the returned {@link GeckoResult}
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
     * This completes the result with the specified value. IllegalStateException is thrown
     * if the result is already complete.
     *
     * @param value The value used to complete the result.
     * @throws IllegalStateException
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
     * This completes the result with the specified {@link Throwable}. IllegalStateException is thrown
     * if the result is already complete.
     *
     * @param exception The {@link Throwable} used to complete the result.
     * @throws IllegalStateException
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
     * @param <T> This is the type of the value delivered via {@link #onValue(Object)}
     * @param <U> This is the type of the value for the result returned from {@link #onValue(Object)}
     */
    public interface OnValueListener<T, U> {
        /**
         * Called when a {@link GeckoResult} is completed with a value. This will be
         * called on the same thread in which the result was completed.
         *
         * @param value The value of the {@link GeckoResult}
         * @return A new {@link GeckoResult}, used for chaining results together.
         *         May be null.
         */
        GeckoResult<U> onValue(T value) throws Throwable;
    }

    /**
     * An interface used to deliver exceptions to listeners of a {@link GeckoResult}
     *
     * @param <V> This is the type of the vale for the result returned from {@link #onException(Throwable)}
     */
    public interface OnExceptionListener<V> {
        GeckoResult<V> onException(Throwable exception) throws Throwable;
    }

    private boolean haveValue() {
        return mComplete && mError == null;
    }

    private boolean haveError() {
        return mComplete && mError != null;
    }
}
