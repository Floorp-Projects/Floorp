package org.mozilla.geckoview.test.util;

import org.mozilla.geckoview.BuildConfig;

import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import androidx.test.platform.app.InstrumentationRegistry;

public class Environment {
    public static final long DEFAULT_TIMEOUT_MILLIS = 30000;
    public static final long DEFAULT_ARM_DEVICE_TIMEOUT_MILLIS = 30000;
    public static final long DEFAULT_ARM_EMULATOR_TIMEOUT_MILLIS = 120000;
    public static final long DEFAULT_X86_DEVICE_TIMEOUT_MILLIS = 30000;
    public static final long DEFAULT_X86_EMULATOR_TIMEOUT_MILLIS = 30000;
    public static final long DEFAULT_IDE_DEBUG_TIMEOUT_MILLIS = 86400000;

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

    public boolean isDebugging() {
        return Debug.isDebuggerConnected();
    }

    public boolean isEmulator() {
        return "generic".equals(Build.DEVICE) || Build.DEVICE.startsWith("generic_");
    }

    public boolean isDebugBuild() {
        return BuildConfig.DEBUG_BUILD;
    }

    public boolean isX86() {
        final String abi;
        if (Build.VERSION.SDK_INT >= 21) {
            abi = Build.SUPPORTED_ABIS[0];
        } else {
            abi = Build.CPU_ABI;
        }

        return abi.startsWith("x86");
    }

    public boolean isFission() {
        return getEnvVar("MOZ_FORCE_ENABLE_FISSION").equals("1");
    }

    public boolean isWebrender() {
        return getEnvVar("MOZ_WEBRENDER").equals("1");
    }

    public boolean isIsolatedProcess() {
        return BuildConfig.MOZ_ANDROID_CONTENT_SERVICE_ISOLATED_PROCESS;
    }

    public long getScaledTimeoutMillis() {
        if (isX86()) {
            return isEmulator() ? DEFAULT_X86_EMULATOR_TIMEOUT_MILLIS
                                : DEFAULT_X86_DEVICE_TIMEOUT_MILLIS;
        }
        return isEmulator() ? DEFAULT_ARM_EMULATOR_TIMEOUT_MILLIS
                            : DEFAULT_ARM_DEVICE_TIMEOUT_MILLIS;
    }

    public long getDefaultTimeoutMillis() {
        return isDebugging() ? DEFAULT_IDE_DEBUG_TIMEOUT_MILLIS
                             : getScaledTimeoutMillis();
    }
}
