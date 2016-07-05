//
//  ActivityHandler.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2013-06-25.
//  Copyright (c) 2013 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

import org.json.JSONObject;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import static com.adjust.sdk.Constants.ACTIVITY_STATE_FILENAME;
import static com.adjust.sdk.Constants.ATTRIBUTION_FILENAME;
import static com.adjust.sdk.Constants.LOGTAG;

public class ActivityHandler extends HandlerThread implements IActivityHandler {

    private static long TIMER_INTERVAL;
    private static long TIMER_START;
    private static long SESSION_INTERVAL;
    private static long SUBSESSION_INTERVAL;
    private static final String TIME_TRAVEL = "Time travel!";
    private static final String ADJUST_PREFIX = "adjust_";
    private static final String ACTIVITY_STATE_NAME = "Activity state";
    private static final String ATTRIBUTION_NAME = "Attribution";

    private SessionHandler sessionHandler;
    private IPackageHandler packageHandler;
    private ActivityState activityState;
    private ILogger logger;
    private static ScheduledExecutorService timer;
    private boolean enabled;
    private boolean offline;

    private DeviceInfo deviceInfo;
    private AdjustConfig adjustConfig; // always valid after construction
    private AdjustAttribution attribution;
    private IAttributionHandler attributionHandler;

    private ActivityHandler(AdjustConfig adjustConfig) {
        super(LOGTAG, MIN_PRIORITY);
        setDaemon(true);
        start();

        logger = AdjustFactory.getLogger();
        sessionHandler = new SessionHandler(getLooper(), this);
        enabled = true;
        init(adjustConfig);

        Message message = Message.obtain();
        message.arg1 = SessionHandler.INIT;
        sessionHandler.sendMessage(message);
    }

    @Override
    public void init(AdjustConfig adjustConfig) {
        this.adjustConfig = adjustConfig;
    }

    public static ActivityHandler getInstance(AdjustConfig adjustConfig) {
        if (adjustConfig == null) {
            AdjustFactory.getLogger().error("AdjustConfig missing");
            return null;
        }

        if (!adjustConfig.isValid()) {
            AdjustFactory.getLogger().error("AdjustConfig not initialized correctly");
            return null;
        }

        ActivityHandler activityHandler = new ActivityHandler(adjustConfig);
        return activityHandler;
    }

    @Override
    public void trackSubsessionStart() {
        Message message = Message.obtain();
        message.arg1 = SessionHandler.START;
        sessionHandler.sendMessage(message);
    }

    @Override
    public void trackSubsessionEnd() {
        Message message = Message.obtain();
        message.arg1 = SessionHandler.END;
        sessionHandler.sendMessage(message);
    }

    @Override
    public void trackEvent(AdjustEvent event) {
        Message message = Message.obtain();
        message.arg1 = SessionHandler.EVENT;
        message.obj = event;
        sessionHandler.sendMessage(message);
    }

    @Override
    public void finishedTrackingActivity(JSONObject jsonResponse) {
        if (jsonResponse == null) {
            return;
        }

        Message message = Message.obtain();
        message.arg1 = SessionHandler.FINISH_TRACKING;
        message.obj = jsonResponse;
        sessionHandler.sendMessage(message);
    }

    @Override
    public void setEnabled(boolean enabled) {
        if (enabled == this.enabled) {
            if (enabled) {
                logger.debug("Adjust already enabled");
            } else {
                logger.debug("Adjust already disabled");
            }
            return;
        }
        this.enabled = enabled;
        if (activityState != null) {
            activityState.enabled = enabled;
        }
        if (enabled) {
            if (toPause()) {
                logger.info("Package and attribution handler remain paused due to the SDK is offline");
            } else {
                logger.info("Resuming package handler and attribution handler to enabled the SDK");
            }
            trackSubsessionStart();
        } else {
            logger.info("Pausing package handler and attribution handler to disable the SDK");
            trackSubsessionEnd();
        }
    }

    @Override
    public void setOfflineMode(boolean offline) {
        if (offline == this.offline) {
            if (offline) {
                logger.debug("Adjust already in offline mode");
            } else {
                logger.debug("Adjust already in online mode");
            }
            return;
        }
        this.offline = offline;
        if (offline) {
            logger.info("Pausing package and attribution handler to put in offline mode");
        } else {
            if (toPause()) {
                logger.info("Package and attribution handler remain paused because the SDK is disabled");
            } else {
                logger.info("Resuming package handler and attribution handler to put in online mode");
            }
        }
        updateStatus();
    }

    @Override
    public boolean isEnabled() {
        if (activityState != null) {
            return activityState.enabled;
        } else {
            return enabled;
        }
    }

    @Override
    public void readOpenUrl(Uri url, long clickTime) {
        Message message = Message.obtain();
        message.arg1 = SessionHandler.DEEP_LINK;
        UrlClickTime urlClickTime = new UrlClickTime(url, clickTime);
        message.obj = urlClickTime;
        sessionHandler.sendMessage(message);
    }

    @Override
    public boolean tryUpdateAttribution(AdjustAttribution attribution) {
        if (attribution == null) return false;

        if (attribution.equals(this.attribution)) {
            return false;
        }

        saveAttribution(attribution);
        launchAttributionListener();
        return true;
    }

    private void saveAttribution(AdjustAttribution attribution) {
        this.attribution = attribution;
        writeAttribution();
    }

    private void launchAttributionListener() {
        if (adjustConfig.onAttributionChangedListener == null) {
            return;
        }
        Handler handler = new Handler(adjustConfig.context.getMainLooper());
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                adjustConfig.onAttributionChangedListener.onAttributionChanged(attribution);
            }
        };
        handler.post(runnable);
    }

    @Override
    public void setAskingAttribution(boolean askingAttribution) {
        activityState.askingAttribution = askingAttribution;
        writeActivityState();
    }

    @Override
    public ActivityPackage getAttributionPackage() {
        long now = System.currentTimeMillis();
        PackageBuilder attributionBuilder = new PackageBuilder(adjustConfig,
                deviceInfo,
                activityState,
                now);
        return attributionBuilder.buildAttributionPackage();
    }

    @Override
    public void sendReferrer(String referrer, long clickTime) {
        Message message = Message.obtain();
        message.arg1 = SessionHandler.SEND_REFERRER;
        ReferrerClickTime referrerClickTime = new ReferrerClickTime(referrer, clickTime);
        message.obj = referrerClickTime;
        sessionHandler.sendMessage(message);
    }

    private class UrlClickTime {
        Uri url;
        long clickTime;

        UrlClickTime(Uri url, long clickTime) {
            this.url = url;
            this.clickTime = clickTime;
        }
    }

    private class ReferrerClickTime {
        String referrer;
        long clickTime;

        ReferrerClickTime(String referrer, long clickTime) {
            this.referrer = referrer;
            this.clickTime = clickTime;
        }
    }

    private void updateStatus() {
        Message message = Message.obtain();
        message.arg1 = SessionHandler.UPDATE_STATUS;
        sessionHandler.sendMessage(message);
    }

    private static final class SessionHandler extends Handler {
        private static final int BASE_ADDRESS = 72630;
        private static final int INIT = BASE_ADDRESS + 1;
        private static final int START = BASE_ADDRESS + 2;
        private static final int END = BASE_ADDRESS + 3;
        private static final int EVENT = BASE_ADDRESS + 4;
        private static final int FINISH_TRACKING = BASE_ADDRESS + 5;
        private static final int DEEP_LINK = BASE_ADDRESS + 6;
        private static final int SEND_REFERRER = BASE_ADDRESS + 7;
        private static final int UPDATE_STATUS = BASE_ADDRESS + 8;

        private final WeakReference<ActivityHandler> sessionHandlerReference;

        protected SessionHandler(Looper looper, ActivityHandler sessionHandler) {
            super(looper);
            this.sessionHandlerReference = new WeakReference<ActivityHandler>(sessionHandler);
        }

        @Override
        public void handleMessage(Message message) {
            super.handleMessage(message);

            ActivityHandler sessionHandler = sessionHandlerReference.get();
            if (sessionHandler == null) {
                return;
            }

            switch (message.arg1) {
                case INIT:
                    sessionHandler.initInternal();
                    break;
                case START:
                    sessionHandler.startInternal();
                    break;
                case END:
                    sessionHandler.endInternal();
                    break;
                case EVENT:
                    AdjustEvent event = (AdjustEvent) message.obj;
                    sessionHandler.trackEventInternal(event);
                    break;
                case FINISH_TRACKING:
                    JSONObject jsonResponse = (JSONObject) message.obj;
                    sessionHandler.finishedTrackingActivityInternal(jsonResponse);
                    break;
                case DEEP_LINK:
                    UrlClickTime urlClickTime = (UrlClickTime) message.obj;
                    sessionHandler.readOpenUrlInternal(urlClickTime.url, urlClickTime.clickTime);
                    break;
                case SEND_REFERRER:
                    ReferrerClickTime referrerClickTime = (ReferrerClickTime) message.obj;
                    sessionHandler.sendReferrerInternal(referrerClickTime.referrer, referrerClickTime.clickTime);
                    break;
                case UPDATE_STATUS:
                    sessionHandler.updateStatusInternal();
                    break;
            }
        }
    }

    private void initInternal() {
        TIMER_INTERVAL = AdjustFactory.getTimerInterval();
        TIMER_START = AdjustFactory.getTimerStart();
        SESSION_INTERVAL = AdjustFactory.getSessionInterval();
        SUBSESSION_INTERVAL = AdjustFactory.getSubsessionInterval();

        deviceInfo = new DeviceInfo(adjustConfig.context, adjustConfig.sdkPrefix);

        if (adjustConfig.environment == AdjustConfig.ENVIRONMENT_PRODUCTION) {
            logger.setLogLevel(LogLevel.ASSERT);
        } else {
            logger.setLogLevel(adjustConfig.logLevel);
        }

        if (adjustConfig.eventBufferingEnabled) {
            logger.info("Event buffering is enabled");
        }

        String playAdId = Util.getPlayAdId(adjustConfig.context);
        if (playAdId == null) {
            logger.info("Unable to get Google Play Services Advertising ID at start time");
        }

        if (adjustConfig.defaultTracker != null) {
            logger.info("Default tracker: '%s'", adjustConfig.defaultTracker);
        }

        if (adjustConfig.referrer != null) {
            sendReferrer(adjustConfig.referrer, adjustConfig.referrerClickTime); // send to background queue to make sure that activityState is valid
        }

        readAttribution();
        readActivityState();

        packageHandler = AdjustFactory.getPackageHandler(this, adjustConfig.context, toPause());

        startInternal();
    }

    private void startInternal() {
        // it shouldn't start if it was disabled after a first session
        if (activityState != null
                && !activityState.enabled) {
            return;
        }

        updateStatusInternal();

        processSession();

        checkAttributionState();

        startTimer();
    }

    private void processSession() {
        long now = System.currentTimeMillis();

        // very first session
        if (activityState == null) {
            activityState = new ActivityState();
            activityState.sessionCount = 1; // this is the first session

            transferSessionPackage(now);
            activityState.resetSessionAttributes(now);
            activityState.enabled = this.enabled;
            writeActivityState();
            return;
        }

        long lastInterval = now - activityState.lastActivity;

        if (lastInterval < 0) {
            logger.error(TIME_TRAVEL);
            activityState.lastActivity = now;
            writeActivityState();
            return;
        }

        // new session
        if (lastInterval > SESSION_INTERVAL) {
            activityState.sessionCount++;
            activityState.lastInterval = lastInterval;

            transferSessionPackage(now);
            activityState.resetSessionAttributes(now);
            writeActivityState();
            return;
        }

        // new subsession
        if (lastInterval > SUBSESSION_INTERVAL) {
            activityState.subsessionCount++;
            activityState.sessionLength += lastInterval;
            activityState.lastActivity = now;
            writeActivityState();
            logger.info("Started subsession %d of session %d",
                    activityState.subsessionCount,
                    activityState.sessionCount);
        }
    }

    private void checkAttributionState() {
        // if there is no attribution saved, or there is one being asked
        if (attribution == null || activityState.askingAttribution) {
            getAttributionHandler().getAttribution();
        }
    }

    private void endInternal() {
        packageHandler.pauseSending();
        getAttributionHandler().pauseSending();
        stopTimer();
        if (updateActivityState(System.currentTimeMillis())) {
            writeActivityState();
        }
    }

    private void trackEventInternal(AdjustEvent event) {
        if (!checkEvent(event)) return;
        if (!activityState.enabled) return;

        long now = System.currentTimeMillis();

        activityState.eventCount++;
        updateActivityState(now);

        PackageBuilder eventBuilder = new PackageBuilder(adjustConfig, deviceInfo, activityState, now);
        ActivityPackage eventPackage = eventBuilder.buildEventPackage(event);
        packageHandler.addPackage(eventPackage);

        if (adjustConfig.eventBufferingEnabled) {
            logger.info("Buffered event %s", eventPackage.getSuffix());
        } else {
            packageHandler.sendFirstPackage();
        }

        writeActivityState();
    }

    private void finishedTrackingActivityInternal(JSONObject jsonResponse) {
        if (jsonResponse == null) {
            return;
        }

        String deeplink = jsonResponse.optString("deeplink", null);
        launchDeeplinkMain(deeplink);
        getAttributionHandler().checkAttribution(jsonResponse);
    }

    private void sendReferrerInternal(String referrer, long clickTime) {
        ActivityPackage clickPackage = buildQueryStringClickPackage(referrer,
                "reftag",
                clickTime);
        if (clickPackage == null) {
            return;
        }

        getAttributionHandler().getAttribution();

        packageHandler.sendClickPackage(clickPackage);
    }

    private void readOpenUrlInternal(Uri url, long clickTime) {
        if (url == null) {
            return;
        }

        String queryString = url.getQuery();

        ActivityPackage clickPackage = buildQueryStringClickPackage(queryString, "deeplink", clickTime);
        if (clickPackage == null) {
            return;
        }

        getAttributionHandler().getAttribution();

        packageHandler.sendClickPackage(clickPackage);
    }

    private ActivityPackage buildQueryStringClickPackage(String queryString, String source, long clickTime) {
        if (queryString == null) {
            return null;
        }

        long now = System.currentTimeMillis();
        Map<String, String> queryStringParameters = new HashMap<String, String>();
        AdjustAttribution queryStringAttribution = new AdjustAttribution();
        boolean hasAdjustTags = false;

        String[] queryPairs = queryString.split("&");
        for (String pair : queryPairs) {
            if (readQueryString(pair, queryStringParameters, queryStringAttribution)) {
                hasAdjustTags = true;
            }
        }

        if (!hasAdjustTags) {
            return null;
        }

        String reftag = queryStringParameters.remove("reftag");

        PackageBuilder builder = new PackageBuilder(adjustConfig, deviceInfo, activityState, now);
        builder.extraParameters = queryStringParameters;
        builder.attribution = queryStringAttribution;
        builder.reftag = reftag;
        ActivityPackage clickPackage = builder.buildClickPackage(source, clickTime);
        return clickPackage;
    }

    private boolean readQueryString(String queryString,
                                    Map<String, String> extraParameters,
                                    AdjustAttribution queryStringAttribution) {
        String[] pairComponents = queryString.split("=");
        if (pairComponents.length != 2) return false;

        String key = pairComponents[0];
        if (!key.startsWith(ADJUST_PREFIX)) return false;

        String value = pairComponents[1];
        if (value.length() == 0) return false;

        String keyWOutPrefix = key.substring(ADJUST_PREFIX.length());
        if (keyWOutPrefix.length() == 0) return false;

        if (!trySetAttribution(queryStringAttribution, keyWOutPrefix, value)) {
            extraParameters.put(keyWOutPrefix, value);
        }

        return true;
    }

    private boolean trySetAttribution(AdjustAttribution queryStringAttribution,
                                      String key,
                                      String value) {
        if (key.equals("tracker")) {
            queryStringAttribution.trackerName = value;
            return true;
        }

        if (key.equals("campaign")) {
            queryStringAttribution.campaign = value;
            return true;
        }

        if (key.equals("adgroup")) {
            queryStringAttribution.adgroup = value;
            return true;
        }

        if (key.equals("creative")) {
            queryStringAttribution.creative = value;
            return true;
        }

        return false;
    }

    private void updateStatusInternal() {
        updateAttributionHandlerStatus();
        updatePackageHandlerStatus();
    }

    private void updateAttributionHandlerStatus() {
        if (attributionHandler == null) {
            return;
        }
        if (toPause()) {
            attributionHandler.pauseSending();
        } else {
            attributionHandler.resumeSending();
        }
    }

    private void updatePackageHandlerStatus() {
        if (packageHandler == null) {
            return;
        }
        if (toPause()) {
            packageHandler.pauseSending();
        } else {
            packageHandler.resumeSending();
        }
    }

    private void launchDeeplinkMain(String deeplink) {
        if (deeplink == null) return;

        Uri location = Uri.parse(deeplink);
        Intent mapIntent = new Intent(Intent.ACTION_VIEW, location);
        mapIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Verify it resolves
        PackageManager packageManager = adjustConfig.context.getPackageManager();
        List<ResolveInfo> activities = packageManager.queryIntentActivities(mapIntent, 0);
        boolean isIntentSafe = activities.size() > 0;

        // Start an activity if it's safe
        if (!isIntentSafe) {
            logger.error("Unable to open deep link (%s)", deeplink);
            return;
        }

        logger.info("Open deep link (%s)", deeplink);
        adjustConfig.context.startActivity(mapIntent);
    }

    private boolean updateActivityState(long now) {
        long lastInterval = now - activityState.lastActivity;
        // ignore late updates
        if (lastInterval > SESSION_INTERVAL) {
            return false;
        }
        activityState.lastActivity = now;

        if (lastInterval < 0) {
            logger.error(TIME_TRAVEL);
        } else {
            activityState.sessionLength += lastInterval;
            activityState.timeSpent += lastInterval;
        }
        return true;
    }

    public static boolean deleteActivityState(Context context) {
        return context.deleteFile(ACTIVITY_STATE_FILENAME);
    }

    public static boolean deleteAttribution(Context context) {
        return context.deleteFile(ATTRIBUTION_FILENAME);
    }

    private void transferSessionPackage(long now) {
        PackageBuilder builder = new PackageBuilder(adjustConfig, deviceInfo, activityState, now);
        ActivityPackage sessionPackage = builder.buildSessionPackage();
        packageHandler.addPackage(sessionPackage);
        packageHandler.sendFirstPackage();
    }

    private void startTimer() {
        stopTimer();

        if (!activityState.enabled) {
            return;
        }
        timer = Executors.newSingleThreadScheduledExecutor();
        timer.scheduleWithFixedDelay(new Runnable() {
            @Override
            public void run() {
                timerFired();
            }
        }, TIMER_START, TIMER_INTERVAL, TimeUnit.MILLISECONDS);
    }

    private void stopTimer() {
        if (timer != null) {
            timer.shutdown();
            timer = null;
        }
    }

    private void timerFired() {
        if (!activityState.enabled) {
            stopTimer();
            return;
        }

        packageHandler.sendFirstPackage();

        if (updateActivityState(System.currentTimeMillis())) {
            writeActivityState();
        }
    }

    private void readActivityState() {
        try {
	    /**
             * Mozilla:
             * readObject is a generic object, and can therefore return arbitrary generic objects
             * that might not match the expected type. Therefore there will be an implicit cast
	     * here, which can fail. Therefore we have to add the catch (ClassCastException)
             * Note: this has been fixed in upstream, we only need this for the version we are still shipping.
             */
            activityState = Util.readObject(adjustConfig.context, ACTIVITY_STATE_FILENAME, ACTIVITY_STATE_NAME);
        } catch (ClassCastException e) {
            activityState = null;
        }
    }

    private void readAttribution() {
        try {
	    /**
             * Mozilla: (same as in readActivityState() )
             * readObject is a generic object, and can therefore return arbitrary generic objects
             * that might not match the expected type. Therefore there will be an implicit cast
	     * here, which can fail. Therefore we have to add the catch (ClassCastException)
             * Note: this has been fixed in upstream, we only need this for the version we are still shipping.
             */
            attribution = Util.readObject(adjustConfig.context, ATTRIBUTION_FILENAME, ATTRIBUTION_NAME);
        } catch (ClassCastException e) {
            activityState = null;
        }
    }

    private void writeActivityState() {
        Util.writeObject(activityState, adjustConfig.context, ACTIVITY_STATE_FILENAME, ACTIVITY_STATE_NAME);
    }

    private void writeAttribution() {
        Util.writeObject(attribution, adjustConfig.context, ATTRIBUTION_FILENAME, ATTRIBUTION_NAME);
    }

    private boolean checkEvent(AdjustEvent event) {
        if (event == null) {
            logger.error("Event missing");
            return false;
        }

        if (!event.isValid()) {
            logger.error("Event not initialized correctly");
            return false;
        }

        return true;
    }

    // lazy initialization to prevent null activity state before first session
    private IAttributionHandler getAttributionHandler() {
        if (attributionHandler == null) {
            ActivityPackage attributionPackage = getAttributionPackage();
            attributionHandler = AdjustFactory.getAttributionHandler(this,
                    attributionPackage,
                    toPause());
        }
        return attributionHandler;
    }

    private boolean toPause() {
        return offline || !isEnabled();
    }
}
