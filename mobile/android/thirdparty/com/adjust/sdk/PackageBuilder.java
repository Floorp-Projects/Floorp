//
//  PackageBuilder.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2013-06-25.
//  Copyright (c) 2013 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import android.text.TextUtils;

import org.json.JSONObject;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

class PackageBuilder {
    private AdjustConfig adjustConfig;
    private DeviceInfo deviceInfo;
    private ActivityState activityState;
    private long createdAt;

    // reattributions
    Map<String, String> extraParameters;
    AdjustAttribution attribution;
    String reftag;

    private static ILogger logger = AdjustFactory.getLogger();

    public PackageBuilder(AdjustConfig adjustConfig,
                          DeviceInfo deviceInfo,
                          ActivityState activityState,
                          long createdAt) {
        this.adjustConfig = adjustConfig;
        this.deviceInfo = deviceInfo;
        this.activityState = activityState.clone();
        this.createdAt = createdAt;
    }

    public ActivityPackage buildSessionPackage() {
        Map<String, String> parameters = getDefaultParameters();
        addDuration(parameters, "last_interval", activityState.lastInterval);
        addString(parameters, "default_tracker", adjustConfig.defaultTracker);

        ActivityPackage sessionPackage = getDefaultActivityPackage();
        sessionPackage.setPath("/session");
        sessionPackage.setActivityKind(ActivityKind.SESSION);
        sessionPackage.setSuffix("");
        sessionPackage.setParameters(parameters);

        return sessionPackage;
    }

    public ActivityPackage buildEventPackage(AdjustEvent event) {
        Map<String, String> parameters = getDefaultParameters();
        addInt(parameters, "event_count", activityState.eventCount);
        addString(parameters, "event_token", event.eventToken);
        addDouble(parameters, "revenue", event.revenue);
        addString(parameters, "currency", event.currency);
        addMapJson(parameters, "callback_params", event.callbackParameters);
        addMapJson(parameters, "partner_params", event.partnerParameters);

        ActivityPackage eventPackage = getDefaultActivityPackage();
        eventPackage.setPath("/event");
        eventPackage.setActivityKind(ActivityKind.EVENT);
        eventPackage.setSuffix(getEventSuffix(event));
        eventPackage.setParameters(parameters);

        return eventPackage;
    }

    public ActivityPackage buildClickPackage(String source, long clickTime) {
        Map<String, String> parameters = getDefaultParameters();

        addString(parameters, "source", source);
        addDate(parameters, "click_time", clickTime);
        addString(parameters, "reftag", reftag);
        addMapJson(parameters, "params", extraParameters);
        injectAttribution(parameters);

        ActivityPackage clickPackage = getDefaultActivityPackage();
        clickPackage.setPath("/sdk_click");
        clickPackage.setActivityKind(ActivityKind.CLICK);
        clickPackage.setSuffix("");
        clickPackage.setParameters(parameters);

        return clickPackage;
    }

    public ActivityPackage buildAttributionPackage() {
        Map<String, String> parameters = getIdsParameters();

        ActivityPackage attributionPackage = getDefaultActivityPackage();
        attributionPackage.setPath("attribution"); // does not contain '/' because of Uri.Builder.appendPath
        attributionPackage.setActivityKind(ActivityKind.ATTRIBUTION);
        attributionPackage.setSuffix("");
        attributionPackage.setParameters(parameters);

        return attributionPackage;
    }

    private ActivityPackage getDefaultActivityPackage() {
        ActivityPackage activityPackage = new ActivityPackage();
        activityPackage.setClientSdk(deviceInfo.clientSdk);
        return activityPackage;
    }

    private Map<String, String> getDefaultParameters() {
        Map<String, String> parameters = new HashMap<String, String>();

        injectDeviceInfo(parameters);
        injectConfig(parameters);
        injectActivityState(parameters);
        addDate(parameters, "created_at", createdAt);

        // general
        checkDeviceIds(parameters);

        return parameters;
    }

    private Map<String, String> getIdsParameters() {
        Map<String, String> parameters = new HashMap<String, String>();

        injectDeviceInfoIds(parameters);
        injectConfig(parameters);
        injectActivityStateIds(parameters);

        checkDeviceIds(parameters);

        return parameters;
    }

    private void injectDeviceInfo(Map<String, String> parameters) {
        injectDeviceInfoIds(parameters);
        addString(parameters, "fb_id", deviceInfo.fbAttributionId);
        addString(parameters, "package_name", deviceInfo.packageName);
        addString(parameters, "app_version", deviceInfo.appVersion);
        addString(parameters, "device_type", deviceInfo.deviceType);
        addString(parameters, "device_name", deviceInfo.deviceName);
        addString(parameters, "device_manufacturer", deviceInfo.deviceManufacturer);
        addString(parameters, "os_name", deviceInfo.osName);
        addString(parameters, "os_version", deviceInfo.osVersion);
        addString(parameters, "language", deviceInfo.language);
        addString(parameters, "country", deviceInfo.country);
        addString(parameters, "screen_size", deviceInfo.screenSize);
        addString(parameters, "screen_format", deviceInfo.screenFormat);
        addString(parameters, "screen_density", deviceInfo.screenDensity);
        addString(parameters, "display_width", deviceInfo.displayWidth);
        addString(parameters, "display_height", deviceInfo.displayHeight);
        fillPluginKeys(parameters);
    }

    private void injectDeviceInfoIds(Map<String, String> parameters) {
        addString(parameters, "mac_sha1", deviceInfo.macSha1);
        addString(parameters, "mac_md5", deviceInfo.macShortMd5);
        addString(parameters, "android_id", deviceInfo.androidId);
    }

    private void injectConfig(Map<String, String> parameters) {
        addString(parameters, "app_token", adjustConfig.appToken);
        addString(parameters, "environment", adjustConfig.environment);
        addBoolean(parameters, "device_known", adjustConfig.knownDevice);
        addBoolean(parameters, "needs_attribution_data", adjustConfig.hasListener());

        String playAdId = Util.getPlayAdId(adjustConfig.context);
        addString(parameters, "gps_adid", playAdId);
        Boolean isTrackingEnabled = Util.isPlayTrackingEnabled(adjustConfig.context);
        addBoolean(parameters, "tracking_enabled", isTrackingEnabled);
    }

    private void injectActivityState(Map<String, String> parameters) {
        injectActivityStateIds(parameters);
        addInt(parameters, "session_count", activityState.sessionCount);
        addInt(parameters, "subsession_count", activityState.subsessionCount);
        addDuration(parameters, "session_length", activityState.sessionLength);
        addDuration(parameters, "time_spent", activityState.timeSpent);
    }

    private void injectActivityStateIds(Map<String, String> parameters) {
        addString(parameters, "android_uuid", activityState.uuid);
    }

    private void injectAttribution(Map<String, String> parameters) {
        if (attribution == null) {
            return;
        }
        addString(parameters, "tracker", attribution.trackerName);
        addString(parameters, "campaign", attribution.campaign);
        addString(parameters, "adgroup", attribution.adgroup);
        addString(parameters, "creative", attribution.creative);
    }

    private void checkDeviceIds(Map<String, String> parameters) {
        if (!parameters.containsKey("mac_sha1")
                && !parameters.containsKey("mac_md5")
                && !parameters.containsKey("android_id")
                && !parameters.containsKey("gps_adid")) {
            logger.error("Missing device id's. Please check if Proguard is correctly set with Adjust SDK");
        }
    }

    private void fillPluginKeys(Map<String, String> parameters) {
        if (deviceInfo.pluginKeys == null) {
            return;
        }

        for (Map.Entry<String, String> entry : deviceInfo.pluginKeys.entrySet()) {
            addString(parameters, entry.getKey(), entry.getValue());
        }
    }

    private String getEventSuffix(AdjustEvent event) {
        if (event.revenue == null) {
            return String.format(" '%s'", event.eventToken);
        } else {
            return String.format(Locale.US, " (%.4f %s, '%s')", event.revenue, event.currency, event.eventToken);
        }
    }

    private void addString(Map<String, String> parameters, String key, String value) {
        if (TextUtils.isEmpty(value)) {
            return;
        }

        parameters.put(key, value);
    }

    private void addInt(Map<String, String> parameters, String key, long value) {
        if (value < 0) {
            return;
        }

        String valueString = Long.toString(value);
        addString(parameters, key, valueString);
    }

    private void addDate(Map<String, String> parameters, String key, long value) {
        if (value < 0) {
            return;
        }

        String dateString = Util.dateFormat(value);
        addString(parameters, key, dateString);
    }

    private void addDuration(Map<String, String> parameters, String key, long durationInMilliSeconds) {
        if (durationInMilliSeconds < 0) {
            return;
        }

        long durationInSeconds = (durationInMilliSeconds + 500) / 1000;
        addInt(parameters, key, durationInSeconds);
    }

    private void addMapJson(Map<String, String> parameters, String key, Map<String, String> map) {
        if (map == null) {
            return;
        }

        if (map.size() == 0) {
            return;
        }

        JSONObject jsonObject = new JSONObject(map);
        String jsonString = jsonObject.toString();

        addString(parameters, key, jsonString);
    }

    private void addBoolean(Map<String, String> parameters, String key, Boolean value) {
        if (value == null) {
            return;
        }

        int intValue = value ? 1 : 0;

        addInt(parameters, key, intValue);
    }

    private void addDouble(Map<String, String> parameters, String key, Double value) {
        if (value == null) return;

        String doubleString = String.format("%.5f", value);

        addString(parameters, key, doubleString);
    }
}
