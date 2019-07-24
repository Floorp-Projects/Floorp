package org.mozilla.geckoview.test.util;

import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.test.TestCrashHandler;

import android.os.Process;
import android.support.test.InstrumentationRegistry;

public class RuntimeCreator {
    private static GeckoRuntime sRuntime;

    public static GeckoRuntime getRuntime() {
        if (sRuntime != null) {
            return sRuntime;
        }

        final GeckoRuntimeSettings.Builder runtimeSettingsBuilder =
                new GeckoRuntimeSettings.Builder();
        runtimeSettingsBuilder.arguments(new String[]{"-purgecaches"})
                .extras(InstrumentationRegistry.getArguments())
                .remoteDebuggingEnabled(true)
                .consoleOutput(true)
                .crashHandler(TestCrashHandler.class);

        sRuntime = GeckoRuntime.create(
                InstrumentationRegistry.getTargetContext(),
                runtimeSettingsBuilder.build());

        sRuntime.setDelegate(new GeckoRuntime.Delegate() {
            @Override
            public void onShutdown() {
                Process.killProcess(Process.myPid());
            }
        });

        return sRuntime;
    }
}
