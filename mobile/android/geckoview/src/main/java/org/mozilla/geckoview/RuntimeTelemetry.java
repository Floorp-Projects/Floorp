/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

/** The telemetry API gives access to telemetry data of the Gecko runtime. */
public final class RuntimeTelemetry {
  protected RuntimeTelemetry() {}

  /**
   * The runtime telemetry metric object.
   *
   * @param <T> type of the underlying metric sample
   */
  public static class Metric<T> {
    /** The runtime metric name. */
    public final @NonNull String name;

    /** The metric values. */
    public final @NonNull T value;

    /* package */ Metric(final String name, final T value) {
      this.name = name;
      this.value = value;
    }

    @Override
    public String toString() {
      return "name: " + name + ", value: " + value;
    }

    // For testing
    protected Metric() {
      name = null;
      value = null;
    }
  }

  /** The Histogram telemetry metric object. */
  public static class Histogram extends Metric<long[]> {
    /** Whether or not this is a Categorical Histogram. */
    public final boolean isCategorical;

    /* package */ Histogram(final boolean isCategorical, final String name, final long[] value) {
      super(name, value);
      this.isCategorical = isCategorical;
    }

    // For testing
    protected Histogram() {
      super(null, null);
      isCategorical = false;
    }
  }

  /**
   * The runtime telemetry delegate. Implement this if you want to receive runtime (Gecko) telemetry
   * and attach it via {@link GeckoRuntimeSettings.Builder#telemetryDelegate}.
   */
  public interface Delegate {
    /**
     * A runtime telemetry histogram metric has been received.
     *
     * @param metric The runtime metric details.
     */
    @AnyThread
    default void onHistogram(final @NonNull Histogram metric) {}

    /**
     * A runtime telemetry boolean scalar has been received.
     *
     * @param metric The runtime metric details.
     */
    @AnyThread
    default void onBooleanScalar(final @NonNull Metric<Boolean> metric) {}

    /**
     * A runtime telemetry long scalar has been received.
     *
     * @param metric The runtime metric details.
     */
    @AnyThread
    default void onLongScalar(final @NonNull Metric<Long> metric) {}

    /**
     * A runtime telemetry string scalar has been received.
     *
     * @param metric The runtime metric details.
     */
    @AnyThread
    default void onStringScalar(final @NonNull Metric<String> metric) {}
  }

  // The proxy connects to telemetry core and forwards telemetry events
  // to the attached delegate.
  /* package */ static final class Proxy extends JNIObject {
    private final Delegate mDelegate;

    public Proxy(final @NonNull Delegate delegate) {
      mDelegate = delegate;
    }

    // Attach to current runtime.
    // We might have different mechanics of attaching to specific runtimes
    // in future, for which case we should split the delegate assignment in
    // the setup phase from the attaching.
    public void attach() {
      if (GeckoThread.isRunning()) {
        registerDelegateProxy(this);
      } else {
        GeckoThread.queueNativeCall(Proxy.class, "registerDelegateProxy", Proxy.class, this);
      }
    }

    public @NonNull Delegate getDelegate() {
      return mDelegate;
    }

    @WrapForJNI(dispatchTo = "gecko")
    private static native void registerDelegateProxy(Proxy proxy);

    @WrapForJNI(calledFrom = "gecko")
    /* package */ void dispatchHistogram(
        final boolean isCategorical, final String name, final long[] values) {
      if (mDelegate == null) {
        // TODO throw?
        return;
      }
      mDelegate.onHistogram(new Histogram(isCategorical, name, values));
    }

    @WrapForJNI(calledFrom = "gecko")
    /* package */ void dispatchStringScalar(final String name, final String value) {
      if (mDelegate == null) {
        return;
      }
      mDelegate.onStringScalar(new Metric<>(name, value));
    }

    @WrapForJNI(calledFrom = "gecko")
    /* package */ void dispatchBooleanScalar(final String name, final boolean value) {
      if (mDelegate == null) {
        return;
      }
      mDelegate.onBooleanScalar(new Metric<>(name, value));
    }

    @WrapForJNI(calledFrom = "gecko")
    /* package */ void dispatchLongScalar(final String name, final long value) {
      if (mDelegate == null) {
        return;
      }
      mDelegate.onLongScalar(new Metric<>(name, value));
    }

    @Override // JNIObject
    protected void disposeNative() {
      // We don't hold native references.
      throw new UnsupportedOperationException();
    }
  }
}
