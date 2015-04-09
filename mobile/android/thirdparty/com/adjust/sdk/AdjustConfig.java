package com.adjust.sdk;

import android.content.Context;

/**
 * Created by pfms on 06/11/14.
 */
public class AdjustConfig {
    Context context;
    String appToken;
    String environment;
    LogLevel logLevel;
    String sdkPrefix;
    Boolean eventBufferingEnabled;
    String defaultTracker;
    OnAttributionChangedListener onAttributionChangedListener;
    String referrer;
    long referrerClickTime;
    Boolean knownDevice;

    public static final String ENVIRONMENT_SANDBOX = "sandbox";
    public static final String ENVIRONMENT_PRODUCTION = "production";

    public AdjustConfig(Context context, String appToken, String environment) {
        if (!isValid(context, appToken, environment)) {
            return;
        }

        this.context = context.getApplicationContext();
        this.appToken = appToken;
        this.environment = environment;

        // default values
        this.logLevel = LogLevel.INFO;
        this.eventBufferingEnabled = false;
    }

    public void setEventBufferingEnabled(Boolean eventBufferingEnabled) {
        this.eventBufferingEnabled = eventBufferingEnabled;
    }

    public void setLogLevel(LogLevel logLevel) {
        this.logLevel = logLevel;
    }

    public void setSdkPrefix(String sdkPrefix) {
        this.sdkPrefix = sdkPrefix;
    }

    public void setDefaultTracker(String defaultTracker) {
        this.defaultTracker = defaultTracker;
    }

    public void setOnAttributionChangedListener(OnAttributionChangedListener onAttributionChangedListener) {
        this.onAttributionChangedListener = onAttributionChangedListener;
    }

    public boolean hasListener() {
        return onAttributionChangedListener != null;
    }

    public boolean isValid() {
        return appToken != null;
    }

    private boolean isValid(Context context, String appToken, String environment) {
        if (!checkAppToken(appToken)) return false;
        if (!checkEnvironment(environment)) return false;
        if (!checkContext(context)) return false;

        return true;
    }

    private static boolean checkContext(Context context) {
        ILogger logger = AdjustFactory.getLogger();
        if (context == null) {
            logger.error("Missing context");
            return false;
        }

        if (!Util.checkPermission(context, android.Manifest.permission.INTERNET)) {
            logger.error("Missing permission: INTERNET");
            return false;
        }

        return true;
    }

    private static boolean checkAppToken(String appToken) {
        ILogger logger = AdjustFactory.getLogger();
        if (appToken == null) {
            logger.error("Missing App Token.");
            return false;
        }

        if (appToken.length() != 12) {
            logger.error("Malformed App Token '%s'", appToken);
            return false;
        }

        return true;
    }

    private static boolean checkEnvironment(String environment) {
        ILogger logger = AdjustFactory.getLogger();
        if (environment == null) {
            logger.error("Missing environment");
            return false;
        }

        if (environment == AdjustConfig.ENVIRONMENT_SANDBOX) {
            logger.Assert("SANDBOX: Adjust is running in Sandbox mode. " +
                    "Use this setting for testing. " +
                    "Don't forget to set the environment to `production` before publishing!");
            return true;
        }
        if (environment == AdjustConfig.ENVIRONMENT_PRODUCTION) {
            logger.Assert(
                    "PRODUCTION: Adjust is running in Production mode. " +
                            "Use this setting only for the build that you want to publish. " +
                            "Set the environment to `sandbox` if you want to test your app!");
            return true;
        }

        logger.error("Unknown environment '%s'", environment);
        return false;
    }
}