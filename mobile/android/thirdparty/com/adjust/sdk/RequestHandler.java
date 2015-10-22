//
//  RequestHandler.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2013-06-25.
//  Copyright (c) 2013 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.NameValuePair;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.entity.UrlEncodedFormEntity;
import ch.boye.httpclientandroidlib.client.methods.HttpPost;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.client.utils.URLEncodedUtils;
import ch.boye.httpclientandroidlib.message.BasicNameValuePair;
import org.json.JSONObject;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.ref.WeakReference;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public class RequestHandler extends HandlerThread implements IRequestHandler {
    private InternalHandler internalHandler;
    private IPackageHandler packageHandler;
    private HttpClient httpClient;
    private ILogger logger;

    public RequestHandler(IPackageHandler packageHandler) {
        super(Constants.LOGTAG, MIN_PRIORITY);
        setDaemon(true);
        start();

        this.logger = AdjustFactory.getLogger();
        this.internalHandler = new InternalHandler(getLooper(), this);
        init(packageHandler);

        Message message = Message.obtain();
        message.arg1 = InternalHandler.INIT;
        internalHandler.sendMessage(message);
    }

    @Override
    public void init(IPackageHandler packageHandler) {
        this.packageHandler = packageHandler;
    }

    @Override
    public void sendPackage(ActivityPackage pack) {
        Message message = Message.obtain();
        message.arg1 = InternalHandler.SEND;
        message.obj = pack;
        internalHandler.sendMessage(message);
    }

    @Override
    public void sendClickPackage(ActivityPackage clickPackage) {
        Message message = Message.obtain();
        message.arg1 = InternalHandler.SEND_CLICK;
        message.obj = clickPackage;
        internalHandler.sendMessage(message);

    }

    private static final class InternalHandler extends Handler {
        private static final int INIT = 72401;
        private static final int SEND = 72400;
        private static final int SEND_CLICK = 72402;

        private final WeakReference<RequestHandler> requestHandlerReference;

        protected InternalHandler(Looper looper, RequestHandler requestHandler) {
            super(looper);
            this.requestHandlerReference = new WeakReference<RequestHandler>(requestHandler);
        }

        @Override
        public void handleMessage(Message message) {
            super.handleMessage(message);

            RequestHandler requestHandler = requestHandlerReference.get();
            if (null == requestHandler) {
                return;
            }

            switch (message.arg1) {
                case INIT:
                    requestHandler.initInternal();
                    break;
                case SEND:
                    ActivityPackage activityPackage = (ActivityPackage) message.obj;
                    requestHandler.sendInternal(activityPackage, true);
                    break;
                case SEND_CLICK:
                    ActivityPackage clickPackage = (ActivityPackage) message.obj;
                    requestHandler.sendInternal(clickPackage, false);
                    break;
            }
        }
    }

    private void initInternal() {
        httpClient = Util.getHttpClient();
    }

    private void sendInternal(ActivityPackage activityPackage, boolean sendToPackageHandler) {
        try {
            HttpUriRequest request = getRequest(activityPackage);
            HttpResponse response = httpClient.execute(request);
            requestFinished(response, sendToPackageHandler);
        } catch (UnsupportedEncodingException e) {
            sendNextPackage(activityPackage, "Failed to encode parameters", e, sendToPackageHandler);
        } catch (ClientProtocolException e) {
            closePackage(activityPackage, "Client protocol error", e, sendToPackageHandler);
        } catch (SocketTimeoutException e) {
            closePackage(activityPackage, "Request timed out", e, sendToPackageHandler);
        } catch (IOException e) {
            closePackage(activityPackage, "Request failed", e, sendToPackageHandler);
        } catch (Throwable e) {
            sendNextPackage(activityPackage, "Runtime exception", e, sendToPackageHandler);
        }
    }

    private void requestFinished(HttpResponse response, boolean sendToPackageHandler) {
        JSONObject jsonResponse = Util.parseJsonResponse(response, logger);

        if (jsonResponse == null) {
            if (sendToPackageHandler) {
                packageHandler.closeFirstPackage();
            }
            return;
        }

        packageHandler.finishedTrackingActivity(jsonResponse);
        if (sendToPackageHandler) {
            packageHandler.sendNextPackage();
        }
    }

    // close current package because it failed
    private void closePackage(ActivityPackage activityPackage, String message, Throwable throwable, boolean sendToPackageHandler) {
        final String packageMessage = activityPackage.getFailureMessage();
        final String handlerMessage = packageHandler.getFailureMessage();
        final String reasonString = getReasonString(message, throwable);
        logger.error("%s. (%s) %s", packageMessage, reasonString, handlerMessage);

        if (sendToPackageHandler) {
            packageHandler.closeFirstPackage();
        }
    }

    // send next package because the current package failed
    private void sendNextPackage(ActivityPackage activityPackage, String message, Throwable throwable, boolean sendToPackageHandler) {
        final String failureMessage = activityPackage.getFailureMessage();
        final String reasonString = getReasonString(message, throwable);
        logger.error("%s. (%s)", failureMessage, reasonString);

        if (sendToPackageHandler) {
            packageHandler.sendNextPackage();
        }
    }

    private String getReasonString(String message, Throwable throwable) {
        if (throwable != null) {
            return String.format("%s: %s", message, throwable);
        } else {
            return String.format("%s", message);
        }
    }

    private HttpUriRequest getRequest(ActivityPackage activityPackage) throws UnsupportedEncodingException {
        String url = Constants.BASE_URL + activityPackage.getPath();
        HttpPost request = new HttpPost(url);

        String language = Locale.getDefault().getLanguage();
        request.addHeader("Client-SDK", activityPackage.getClientSdk());
        request.addHeader("Accept-Language", language);

        List<NameValuePair> pairs = new ArrayList<NameValuePair>();
        for (Map.Entry<String, String> entry : activityPackage.getParameters().entrySet()) {
            NameValuePair pair = new BasicNameValuePair(entry.getKey(), entry.getValue());
            pairs.add(pair);
        }

        long now = System.currentTimeMillis();
        String dateString = Util.dateFormat(now);
        NameValuePair sentAtPair = new BasicNameValuePair("sent_at", dateString);
        pairs.add(sentAtPair);

        UrlEncodedFormEntity entity = new UrlEncodedFormEntity(pairs);
        entity.setContentType(URLEncodedUtils.CONTENT_TYPE);
        request.setEntity(entity);

        return request;
    }
}
