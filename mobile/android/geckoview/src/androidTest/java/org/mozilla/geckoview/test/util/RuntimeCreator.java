/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test.util;

import static org.mozilla.geckoview.ContentBlocking.SafeBrowsingProvider;

import android.os.Process;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.test.platform.app.InstrumentationRegistry;
import java.util.concurrent.atomic.AtomicInteger;
import org.json.JSONObject;
import org.mozilla.geckoview.ContentBlocking;
import org.mozilla.geckoview.ExperimentDelegate;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.RuntimeTelemetry;
import org.mozilla.geckoview.WebExtension;
import org.mozilla.geckoview.test.TestCrashHandler;

public class RuntimeCreator {
  public static final int TEST_SUPPORT_INITIAL = 0;
  public static final int TEST_SUPPORT_OK = 1;
  public static final int TEST_SUPPORT_ERROR = 2;
  public static final String TEST_SUPPORT_EXTENSION_ID = "test-support@tests.mozilla.org";
  private static final String LOGTAG = "RuntimeCreator";

  private static final Environment env = new Environment();
  private static GeckoRuntime sRuntime;
  public static AtomicInteger sTestSupport = new AtomicInteger(0);
  public static WebExtension sTestSupportExtension;

  // The RuntimeTelemetry.Delegate can only be set when creating the RuntimeCreator, to
  // let tests set their own Delegate we need to create a proxy here.
  public static class RuntimeTelemetryDelegate implements RuntimeTelemetry.Delegate {
    public RuntimeTelemetry.Delegate delegate = null;

    @Override
    public void onHistogram(@NonNull final RuntimeTelemetry.Histogram metric) {
      if (delegate != null) {
        delegate.onHistogram(metric);
      }
    }

    @Override
    public void onBooleanScalar(@NonNull final RuntimeTelemetry.Metric<Boolean> metric) {
      if (delegate != null) {
        delegate.onBooleanScalar(metric);
      }
    }

    @Override
    public void onStringScalar(@NonNull final RuntimeTelemetry.Metric<String> metric) {
      if (delegate != null) {
        delegate.onStringScalar(metric);
      }
    }

    @Override
    public void onLongScalar(@NonNull final RuntimeTelemetry.Metric<Long> metric) {
      if (delegate != null) {
        delegate.onLongScalar(metric);
      }
    }
  }

  /**
   * The ExperimentDelegate can only be set when starting the RuntimeCreator, so for testing we are
   * setting up a proxy here
   */
  public static class RuntimeExperimentDelegate implements ExperimentDelegate {
    public ExperimentDelegate delegate = null;

    @Override
    public GeckoResult<JSONObject> onGetExperimentFeature(@NonNull String feature) {
      if (delegate != null) {
        return delegate.onGetExperimentFeature(feature);
      }
      return ExperimentDelegate.super.onGetExperimentFeature(feature);
    }

    @Override
    public GeckoResult<Void> onRecordExposureEvent(@NonNull String feature) {
      if (delegate != null) {
        return delegate.onRecordExposureEvent(feature);
      }
      return ExperimentDelegate.super.onRecordExposureEvent(feature);
    }

    @Override
    public GeckoResult<Void> onRecordExperimentExposureEvent(
        @NonNull String feature, @NonNull String slug) {
      if (delegate != null) {
        return delegate.onRecordExperimentExposureEvent(feature, slug);
      }
      return ExperimentDelegate.super.onRecordExperimentExposureEvent(feature, slug);
    }

    @Override
    public GeckoResult<Void> onRecordMalformedConfigurationEvent(
        @NonNull String feature, @NonNull String part) {
      if (delegate != null) {
        return delegate.onRecordMalformedConfigurationEvent(feature, part);
      }
      return ExperimentDelegate.super.onRecordMalformedConfigurationEvent(feature, part);
    }
  }

  public static final RuntimeTelemetryDelegate sRuntimeTelemetryProxy =
      new RuntimeTelemetryDelegate();

  public static RuntimeExperimentDelegate sRuntimeExperimentDelegateProxy =
      new RuntimeExperimentDelegate();
  private static WebExtension.Port sBackgroundPort;

  private static WebExtension.PortDelegate sPortDelegate;

  private static WebExtension.MessageDelegate sMessageDelegate =
      new WebExtension.MessageDelegate() {
        @Nullable
        @Override
        public void onConnect(@NonNull final WebExtension.Port port) {
          sBackgroundPort = port;
          port.setDelegate(sWrapperPortDelegate);
        }
      };

  private static WebExtension.PortDelegate sWrapperPortDelegate =
      new WebExtension.PortDelegate() {
        @Override
        public void onPortMessage(
            @NonNull final Object message, @NonNull final WebExtension.Port port) {
          if (sPortDelegate != null) {
            sPortDelegate.onPortMessage(message, port);
          }
        }
      };

  public static WebExtension.Port backgroundPort() {
    return sBackgroundPort;
  }

  public static void registerTestSupport() {
    sTestSupport.set(0);

    sRuntime
        .getWebExtensionController()
        .installBuiltIn("resource://android/assets/web_extensions/test-support/")
        .accept(
            extension -> {
              extension.setMessageDelegate(sMessageDelegate, "browser");
              sTestSupportExtension = extension;
              sTestSupport.set(TEST_SUPPORT_OK);
            },
            exception -> {
              Log.e(LOGTAG, "Could not register TestSupport", exception);
              sTestSupport.set(TEST_SUPPORT_ERROR);
            });
  }

  /**
   * Set the {@link RuntimeTelemetry.Delegate} instance for this test. Application code can only
   * register this delegate when the {@link GeckoRuntime} is created, so we need to proxy it for
   * test code.
   *
   * @param delegate the {@link RuntimeTelemetry.Delegate} for this test run.
   */
  public static void setTelemetryDelegate(final RuntimeTelemetry.Delegate delegate) {
    sRuntimeTelemetryProxy.delegate = delegate;
  }

  /**
   * Set the {@link ExperimentDelegate} instance for this test. Application code can only register
   * this delegate when the {@link GeckoRuntime} is created, so we need to proxy it for test code.
   *
   * @param delegate the {@link ExperimentDelegate} for this test to use.
   */
  public static void setExperimentDelegate(final ExperimentDelegate delegate) {
    sRuntimeExperimentDelegateProxy.delegate = delegate;
  }

  public static void setPortDelegate(final WebExtension.PortDelegate portDelegate) {
    sPortDelegate = portDelegate;
  }

  @UiThread
  public static GeckoRuntime getRuntime() {
    if (sRuntime != null) {
      return sRuntime;
    }

    final SafeBrowsingProvider googleLegacy =
        SafeBrowsingProvider.from(ContentBlocking.GOOGLE_LEGACY_SAFE_BROWSING_PROVIDER)
            .getHashUrl("http://mochi.test:8888/safebrowsing-dummy/gethash")
            .updateUrl("http://mochi.test:8888/safebrowsing-dummy/update")
            .build();

    final SafeBrowsingProvider google =
        SafeBrowsingProvider.from(ContentBlocking.GOOGLE_SAFE_BROWSING_PROVIDER)
            .getHashUrl("http://mochi.test:8888/safebrowsing4-dummy/gethash")
            .updateUrl("http://mochi.test:8888/safebrowsing4-dummy/update")
            .build();

    final GeckoRuntimeSettings runtimeSettings =
        new GeckoRuntimeSettings.Builder()
            .contentBlocking(
                new ContentBlocking.Settings.Builder()
                    .safeBrowsingProviders(googleLegacy, google)
                    .build())
            .arguments(new String[] {"-purgecaches"})
            .extras(InstrumentationRegistry.getArguments())
            .remoteDebuggingEnabled(true)
            .consoleOutput(true)
            .crashHandler(TestCrashHandler.class)
            .telemetryDelegate(sRuntimeTelemetryProxy)
            .experimentDelegate(sRuntimeExperimentDelegateProxy)
            .build();

    sRuntime =
        GeckoRuntime.create(
            InstrumentationRegistry.getInstrumentation().getTargetContext(), runtimeSettings);

    registerTestSupport();

    sRuntime.setDelegate(() -> Process.killProcess(Process.myPid()));

    return sRuntime;
  }
}
