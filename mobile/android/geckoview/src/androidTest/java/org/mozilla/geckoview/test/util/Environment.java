package org.mozilla.geckoview.test.util;

import org.mozilla.geckoview.BuildConfig;

import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.support.test.InstrumentationRegistry;

public class Environment {
    private String getEnvVar(final String name) {
        final int nameLen = name.length();
        final Bundle args = InstrumentationRegistry.getArguments();
        String env = args.getString("env0", null);
        for (int i = 1; env != null; i++) {
            if (env.length() >= nameLen + 1 &&
                    env.startsWith(name) &&
                    env.charAt(nameLen) == '=') {
                return env.substring(nameLen + 1);
            }
            env = args.getString("env" + i, null);
        }
        return "";
    }

    public boolean isAutomation() {
        return !getEnvVar("MOZ_IN_AUTOMATION").isEmpty();
    }

    public boolean shouldShutdownOnCrash() {
        return !getEnvVar("MOZ_CRASHREPORTER_SHUTDOWN").isEmpty();
    }

    public boolean isMultiprocess() {
        return Boolean.valueOf(InstrumentationRegistry.getArguments()
                .getString("use_multiprocess",
                        "true"));
    }

    public boolean isDebugging() {
        return Debug.isDebuggerConnected();
    }

    public boolean isEmulator() {
        return "generic".equals(Build.DEVICE) || Build.DEVICE.startsWith("generic_");
    }

    public boolean isDebugBuild() {
        return BuildConfig.DEBUG_BUILD;
    }

    public String getCPUArch() {
        return BuildConfig.ANDROID_CPU_ARCH;
    }
}
