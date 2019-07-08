package org.mozilla.geckoview.test.util;

import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.WebExtension;
import org.mozilla.geckoview.test.TestCrashHandler;

import android.os.Process;
import android.support.annotation.UiThread;
import android.support.test.InstrumentationRegistry;
import android.util.Log;

import java.util.concurrent.atomic.AtomicInteger;

public class RuntimeCreator {
    public static final int TEST_SUPPORT_INITIAL = 0;
    public static final int TEST_SUPPORT_OK = 1;
    public static final int TEST_SUPPORT_ERROR = 2;

    private static GeckoRuntime sRuntime;
    private static final String LOGTAG = "RuntimeCreator";

    public static AtomicInteger sTestSupport = new AtomicInteger(TEST_SUPPORT_INITIAL);
    public static WebExtension sTestSupportWebExtension;

    public static void registerTestSupport() {
        sTestSupport.set(0);
        sRuntime.registerWebExtension(sTestSupportWebExtension)
                .accept(value -> {
                    sTestSupport.set(TEST_SUPPORT_OK);
                }, exception -> {
                    Log.e(LOGTAG, "Could not register TestSupport", exception);
                    sTestSupport.set(TEST_SUPPORT_ERROR);
                });
    }

    @UiThread
    public static GeckoRuntime getRuntime() {
        if (sRuntime != null) {
            return sRuntime;
        }

        final GeckoRuntimeSettings.Builder runtimeSettingsBuilder =
                new GeckoRuntimeSettings.Builder();
        runtimeSettingsBuilder.arguments(new String[]{"-purgecaches"})
                .extras(InstrumentationRegistry.getArguments())
                .remoteDebuggingEnabled(true)
                .consoleOutput(true);

        if (new Environment().isAutomation()) {
            runtimeSettingsBuilder.crashHandler(TestCrashHandler.class);
        }

        sRuntime = GeckoRuntime.create(
                InstrumentationRegistry.getTargetContext(),
                runtimeSettingsBuilder.build());

        sTestSupportWebExtension =
                new WebExtension("resource://android/assets/web_extensions/test-support/",
                        "test-support@mozilla.com",
                        WebExtension.Flags.ALLOW_CONTENT_MESSAGING);

        registerTestSupport();

        sRuntime.setDelegate(() -> Process.killProcess(Process.myPid()));

        return sRuntime;
    }
}
