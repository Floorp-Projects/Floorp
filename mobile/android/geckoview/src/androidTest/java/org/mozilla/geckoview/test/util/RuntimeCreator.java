package org.mozilla.geckoview.test.util;

import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.RuntimeTelemetry;
import org.mozilla.geckoview.WebExtension;
import org.mozilla.geckoview.test.TestCrashHandler;

import android.os.Process;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.support.test.InstrumentationRegistry;
import android.util.Log;

import java.util.concurrent.atomic.AtomicInteger;

public class RuntimeCreator {
    public static final int TEST_SUPPORT_INITIAL = 0;
    public static final int TEST_SUPPORT_OK = 1;
    public static final int TEST_SUPPORT_ERROR = 2;
    private static final String LOGTAG = "RuntimeCreator";

    private static GeckoRuntime sRuntime;
    public static AtomicInteger sTestSupport = new AtomicInteger(0);
    public static final WebExtension TEST_SUPPORT_WEB_EXTENSION =
            new WebExtension("resource://android/assets/web_extensions/test-support/",
                    "test-support@mozilla.com",
                    WebExtension.Flags.ALLOW_CONTENT_MESSAGING);

    // The RuntimeTelemetry.Delegate can only be set when creating the RuntimeCreator, to
    // let tests set their own Delegate we need to create a proxy here.
    public static class RuntimeTelemetryDelegate implements RuntimeTelemetry.Delegate {
        public RuntimeTelemetry.Delegate delegate = null;

        @Override
        public void onTelemetryReceived(@NonNull RuntimeTelemetry.Metric metric) {
            if (delegate != null) {
                delegate.onTelemetryReceived(metric);
            }
        }
    }

    public static final RuntimeTelemetryDelegate sRuntimeTelemetryProxy =
            new RuntimeTelemetryDelegate();

    private static WebExtension.Port sBackgroundPort;

    private static WebExtension.PortDelegate sPortDelegate;

    private static WebExtension.MessageDelegate sMessageDelegate
            = new WebExtension.MessageDelegate() {
        @Nullable
        @Override
        public void onConnect(@NonNull WebExtension.Port port) {
            sBackgroundPort = port;
            port.setDelegate(sWrapperPortDelegate);
        }
    };

    private static WebExtension.PortDelegate sWrapperPortDelegate = new WebExtension.PortDelegate() {
        @Override
        public void onPortMessage(@NonNull Object message, @NonNull WebExtension.Port port) {
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
        sRuntime.registerWebExtension(TEST_SUPPORT_WEB_EXTENSION)
                .accept(value -> {
                    sTestSupport.set(TEST_SUPPORT_OK);
                }, exception -> {
                    Log.e(LOGTAG, "Could not register TestSupport", exception);
                    sTestSupport.set(TEST_SUPPORT_ERROR);
                });
    }

    /**
     * Set the {@link RuntimeTelemetry.Delegate} instance for this test. Application code can only
     * register this delegate when the {@link GeckoRuntime} is created, so we need to proxy it
     * for test code.
     *
     * @param delegate the {@link RuntimeTelemetry.Delegate} for this test run.
     */
    public static void setTelemetryDelegate(RuntimeTelemetry.Delegate delegate) {
        sRuntimeTelemetryProxy.delegate = delegate;
    }

    public static void setPortDelegate(WebExtension.PortDelegate portDelegate) {
        sPortDelegate = portDelegate;
    }

    @UiThread
    public static GeckoRuntime getRuntime() {
        if (sRuntime != null) {
            return sRuntime;
        }

        TEST_SUPPORT_WEB_EXTENSION.setMessageDelegate(sMessageDelegate, "browser");

        final GeckoRuntimeSettings runtimeSettings = new GeckoRuntimeSettings.Builder()
                .arguments(new String[]{"-purgecaches"})
                .extras(InstrumentationRegistry.getArguments())
                .remoteDebuggingEnabled(true)
                .consoleOutput(true)
                .crashHandler(TestCrashHandler.class)
                .telemetryDelegate(sRuntimeTelemetryProxy)
                .build();

        sRuntime = GeckoRuntime.create(
                InstrumentationRegistry.getTargetContext(),
                runtimeSettings);

        registerTestSupport();

        sRuntime.setDelegate(() -> Process.killProcess(Process.myPid()));

        return sRuntime;
    }
}
