package com.adjust.sdk;

import android.net.Uri;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import org.json.JSONObject;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/**
 * Created by pfms on 07/11/14.
 */
public class AttributionHandler implements IAttributionHandler {
    private ScheduledExecutorService scheduler;
    private IActivityHandler activityHandler;
    private ILogger logger;
    private ActivityPackage attributionPackage;
    private ScheduledFuture waitingTask;
    private HttpClient httpClient;
    private boolean paused;

    public AttributionHandler(IActivityHandler activityHandler,
                              ActivityPackage attributionPackage,
                              boolean startPaused) {
        scheduler = Executors.newSingleThreadScheduledExecutor();
        logger = AdjustFactory.getLogger();
        httpClient = Util.getHttpClient();
        init(activityHandler, attributionPackage, startPaused);
    }

    @Override
    public void init(IActivityHandler activityHandler,
                     ActivityPackage attributionPackage,
                     boolean startPaused) {
        this.activityHandler = activityHandler;
        this.attributionPackage = attributionPackage;
        this.paused = startPaused;
    }

    @Override
    public void getAttribution() {
        getAttribution(0);
    }

    @Override
    public void checkAttribution(final JSONObject jsonResponse) {
        scheduler.submit(new Runnable() {
            @Override
            public void run() {
                checkAttributionInternal(jsonResponse);
            }
        });
    }

    @Override
    public void pauseSending() {
        paused = true;
    }

    @Override
    public void resumeSending() {
        paused = false;
    }

    private void getAttribution(int delayInMilliseconds) {
        if (waitingTask != null) {
            waitingTask.cancel(false);
        }

        if (delayInMilliseconds != 0) {
            logger.debug("Waiting to query attribution in %d milliseconds", delayInMilliseconds);
        }

        waitingTask = scheduler.schedule(new Runnable() {
            @Override
            public void run() {
                getAttributionInternal();
            }
        }, delayInMilliseconds, TimeUnit.MILLISECONDS);
    }

    private void checkAttributionInternal(JSONObject jsonResponse) {
        if (jsonResponse == null) return;

        JSONObject attributionJson = jsonResponse.optJSONObject("attribution");
        AdjustAttribution attribution = AdjustAttribution.fromJson(attributionJson);

        int timerMilliseconds = jsonResponse.optInt("ask_in", -1);

        // without ask_in attribute
        if (timerMilliseconds < 0) {
            activityHandler.tryUpdateAttribution(attribution);

            activityHandler.setAskingAttribution(false);

            return;
        }

        activityHandler.setAskingAttribution(true);

        getAttribution(timerMilliseconds);
    }

    private void getAttributionInternal() {
        if (paused) {
            logger.debug("Attribution Handler is paused");
            return;
        }
        logger.verbose("%s", attributionPackage.getExtendedString());
        HttpResponse httpResponse = null;
        try {
            HttpGet request = getRequest(attributionPackage);
            httpResponse = httpClient.execute(request);
        } catch (Exception e) {
            logger.error("Failed to get attribution (%s)", e.getMessage());
            return;
        }

        JSONObject jsonResponse = Util.parseJsonResponse(httpResponse, logger);

        checkAttributionInternal(jsonResponse);
    }

    private Uri buildUri(ActivityPackage attributionPackage) {
        Uri.Builder uriBuilder = new Uri.Builder();

        uriBuilder.scheme(Constants.SCHEME);
        uriBuilder.authority(Constants.AUTHORITY);
        uriBuilder.appendPath(attributionPackage.getPath());

        for (Map.Entry<String, String> entry : attributionPackage.getParameters().entrySet()) {
            uriBuilder.appendQueryParameter(entry.getKey(), entry.getValue());
        }

        return uriBuilder.build();
    }

    private HttpGet getRequest(ActivityPackage attributionPackage) throws URISyntaxException {
        HttpGet request = new HttpGet();
        Uri uri = buildUri(attributionPackage);
        request.setURI(new URI(uri.toString()));

        request.addHeader("Client-SDK", attributionPackage.getClientSdk());

        return request;
    }
}
